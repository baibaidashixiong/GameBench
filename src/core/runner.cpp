#include "core/runner.hpp"

#include "platform/cpu_affinity.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <unordered_set>

#include <sched.h>

namespace gamebench::core {

void BenchmarkRunner::execute_iterations(benchmark::Workload& workload,
                                         const benchmark::WorkloadConfig& workload_config,
                                         std::uint64_t iterations, bool measure, BenchmarkRun& run,
                                         std::vector<double>* worker_busy_samples,
                                         std::vector<double>* total_cpu_work_samples,
                                         std::vector<benchmark::ProcessMetrics>* process_samples) {
  for (std::uint64_t iteration = 0; iteration < iterations; ++iteration) {
    const auto start = SteadyClock::now();
    const auto cpu_start = current_thread_cpu_time_ms();
    const auto cpu_before = sched_getcpu();
    const auto output = workload.execute(workload_config);
    const auto end = SteadyClock::now();
    const auto cpu_end = current_thread_cpu_time_ms();
    const auto cpu_after = sched_getcpu();

    if (output.skipped) {
      run.skipped = true;
      run.validation_message = output.validation_message;
    }
    if (!output.valid) {
      run.valid = false;
      run.validation_message = output.validation_message;
    }
    if (output.scheduling) {
      run.scheduling = output.scheduling;
    }
    if (output.vulkan) {
      run.vulkan = output.vulkan;
    }
    if (output.process) {
      run.process = output.process;
    }
    if (!output.simd_backend.empty()) {
      run.simd_backend = output.simd_backend;
    }
    if (!output.execution_mode.empty()) {
      run.worker_count = output.worker_count;
      run.execution_mode = output.execution_mode;
    }
    if (!output.warnings.empty()) {
      run.warnings = output.warnings;
    }
    if (iteration == 0) {
      run.checksum = output.checksum;
    } else {
      run.checksum = run.checksum * 1099511628211ULL + output.checksum;
    }
    if (measure) {
      run.samples_ms.push_back(output.measured_time_ms.value_or(milliseconds_since(start, end)));
      const auto main_thread_cpu_ms = cpu_end - cpu_start;
      run.main_thread_cpu_ms.push_back(main_thread_cpu_ms);
      if (output.scheduling) {
        if (worker_busy_samples != nullptr) {
          worker_busy_samples->push_back(output.scheduling->worker_busy_ms);
        }
        if (total_cpu_work_samples != nullptr) {
          total_cpu_work_samples->push_back(main_thread_cpu_ms + output.scheduling->worker_busy_ms);
        }
      } else if (output.process) {
        if (process_samples != nullptr) {
          auto process = *output.process;
          process.coordinator_cpu_ms = main_thread_cpu_ms;
          process_samples->push_back(process);
        }
        if (total_cpu_work_samples != nullptr) {
          total_cpu_work_samples->push_back(main_thread_cpu_ms + output.process->child_cpu_ms);
        }
      }
      if (cpu_before >= 0 && cpu_after >= 0 && cpu_before != cpu_after) {
        ++run.main_thread_cpu_migrations;
      }
      if (!output.skipped) {
        run.operations += output.operations;
      }
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
    if (!config.cpu_affinity.empty() && thread_count > config.cpu_affinity.size()) {
      throw std::invalid_argument("CPU Affinity 数量不能少于线程数量");
    }
  }

  const std::vector<unsigned> main_affinity =
      config.cpu_affinity.empty() ? std::vector<unsigned>{}
                                  : std::vector<unsigned>{config.cpu_affinity.front()};
  const platform::ScopedThreadAffinity affinity(main_affinity);

  BenchmarkResult result;
  result.benchmark_id = std::string{workload.id()};
  result.benchmark_version = std::string{workload.version()};
  result.config = config;
  if (workload.id() == "game-frame") {
    result.frame_profile = benchmark::find_frame_profile(config.profile);
  }
  result.system = platform::collect_system_info();
  result.topology = platform::detect_cpu_topology();
  result.valid = true;
  profiler::HardwareCounterSession hardware_session(config.hardware_counters);

  for (const auto thread_count : config.thread_counts) {
    BenchmarkRun run_result;
    run_result.thread_count = thread_count;
    run_result.execution_mode = "serial-main";
    run_result.valid = true;
    run_result.operations = 0;

    const benchmark::WorkloadConfig workload_config{
        .profile_id = config.profile,
        .thread_count = thread_count,
        .seed = config.seed,
        .objects = config.objects,
        .iterations = config.iterations,
        .scheduler = config.scheduler,
        .layout = config.layout,
        .memory_pattern = config.memory_pattern,
        .working_set_bytes = config.working_set_bytes,
        .vulkan_mode = config.vulkan_mode,
        .audio_buffer_size = config.audio_buffer_size,
        .sample_rate = config.sample_rate,
        .cpu_affinity = config.cpu_affinity,
    };

    execute_iterations(workload, workload_config, config.warmup_iterations, false, run_result);
    run_result.checksum = 0;
    run_result.operations = 0;
    std::vector<double> worker_busy_samples;
    std::vector<double> total_cpu_work_samples;
    std::vector<benchmark::ProcessMetrics> process_samples;
    execute_iterations(workload, workload_config, config.repetitions, true, run_result,
                       &worker_busy_samples, &total_cpu_work_samples, &process_samples);
    run_result.statistics = calculate_statistics(run_result.samples_ms);
    run_result.main_thread_time_ms = calculate_statistics(run_result.main_thread_cpu_ms).median_ms;
    run_result.critical_path_time_ms = run_result.statistics.median_ms;
    if (!worker_busy_samples.empty()) {
      run_result.worker_thread_time_ms = calculate_statistics(worker_busy_samples).median_ms;
    }
    if (!process_samples.empty()) {
      run_result.process = process_samples.front();
      auto median = [&process_samples](auto member) {
        std::vector<double> values;
        values.reserve(process_samples.size());
        for (const auto& sample : process_samples) {
          values.push_back(sample.*member);
        }
        return calculate_statistics(values).median_ms;
      };
      run_result.process->coordinator_cpu_ms =
          median(&benchmark::ProcessMetrics::coordinator_cpu_ms);
      run_result.process->main_process_cpu_ms =
          median(&benchmark::ProcessMetrics::main_process_cpu_ms);
      run_result.process->auxiliary_cpu_ms = median(&benchmark::ProcessMetrics::auxiliary_cpu_ms);
      run_result.process->child_cpu_ms = median(&benchmark::ProcessMetrics::child_cpu_ms);
      run_result.process->peak_rss_kb =
          std::ranges::max(process_samples, {}, &benchmark::ProcessMetrics::peak_rss_kb)
              .peak_rss_kb;
    }
    if (!total_cpu_work_samples.empty()) {
      run_result.total_cpu_work_ms = calculate_statistics(total_cpu_work_samples).median_ms;
    }
    result.valid = result.valid && (run_result.valid || run_result.skipped);
    result.runs.push_back(std::move(run_result));
  }

  const auto baseline = std::ranges::find_if(
      result.runs, [](const BenchmarkRun& run) { return run.thread_count == 1; });
  const auto& reference = baseline != result.runs.end() ? *baseline : result.runs.front();
  result.scaling_reference_thread_count = reference.thread_count;
  for (const auto& run : result.runs) {
    const double speedup = reference.statistics.median_ms / run.statistics.median_ms;
    const double normalized_threads = static_cast<double>(run.thread_count) /
                                      static_cast<double>(result.scaling_reference_thread_count);
    result.thread_scaling.push_back({.thread_count = run.thread_count,
                                     .speedup = speedup,
                                     .efficiency = speedup / normalized_threads});
  }

  if (config.hardware_counters) {
    result.hardware_counters = hardware_session.stop();
  }

  return result;
}

} // namespace gamebench::core
