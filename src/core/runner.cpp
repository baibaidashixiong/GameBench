#include "core/runner.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace gamebench::core {

void BenchmarkRunner::execute_iterations(benchmark::Workload& workload,
                                         const benchmark::WorkloadConfig& workload_config,
                                         std::uint64_t iterations, bool measure,
                                         BenchmarkRun& run) {
  for (std::uint64_t iteration = 0; iteration < iterations; ++iteration) {
    const auto start = SteadyClock::now();
    const auto output = workload.execute(workload_config);
    const auto end = SteadyClock::now();

    if (!output.valid) {
      run.valid = false;
      run.validation_message = output.validation_message;
    }
    if (iteration == 0) {
      run.checksum = output.checksum;
    } else {
      run.checksum = run.checksum * 1099511628211ULL + output.checksum;
    }
    if (measure) {
      run.samples_ms.push_back(milliseconds_since(start, end));
    }
  }
}

auto BenchmarkRunner::run(benchmark::Workload& workload, const RunConfig& config)
    -> BenchmarkResult {
  if (config.thread_counts.empty()) {
    throw std::invalid_argument("线程数量不能为空");
  }
  if (config.repetitions == 0) {
    throw std::invalid_argument("repetitions 必须大于零");
  }
  if (config.iterations == 0) {
    throw std::invalid_argument("iterations 必须大于零");
  }
  if (config.objects == 0) {
    throw std::invalid_argument("objects 必须大于零");
  }

  std::unordered_set<unsigned> unique_threads;
  for (const auto thread_count : config.thread_counts) {
    if (thread_count == 0 || !unique_threads.insert(thread_count).second) {
      throw std::invalid_argument("线程数量必须是非零且互不重复的正整数");
    }
  }

  BenchmarkResult result;
  result.benchmark_id = std::string{workload.id()};
  result.benchmark_version = std::string{workload.version()};
  result.config = config;
  result.system = platform::collect_system_info();
  result.valid = true;

  for (const auto thread_count : config.thread_counts) {
    BenchmarkRun run_result;
    run_result.thread_count = thread_count;
    run_result.valid = true;
    run_result.operations = 0;

    const benchmark::WorkloadConfig workload_config{
        .thread_count = thread_count,
        .seed = config.seed,
        .objects = config.objects,
        .iterations = config.iterations,
    };

    execute_iterations(workload, workload_config, config.warmup_iterations, false, run_result);
    run_result.checksum = 0;
    execute_iterations(workload, workload_config, config.repetitions, true, run_result);
    run_result.statistics = calculate_statistics(run_result.samples_ms);
    run_result.operations = config.objects * config.iterations * config.repetitions;
    result.valid = result.valid && run_result.valid;
    result.runs.push_back(std::move(run_result));
  }

  const auto baseline = std::ranges::find_if(
      result.runs, [](const BenchmarkRun& run) { return run.thread_count == 1; });
  const auto& reference = baseline != result.runs.end() ? *baseline : result.runs.front();
  for (const auto& run : result.runs) {
    const double speedup = reference.statistics.median_ms / run.statistics.median_ms;
    result.thread_scaling.push_back(
        {.thread_count = run.thread_count,
         .speedup = speedup,
         .efficiency = speedup / static_cast<double>(run.thread_count)});
  }

  return result;
}

} // namespace gamebench::core

