#include "benchmark/scoring.hpp"

namespace gamebench::benchmark {

auto calculate_scores(const core::BenchmarkResult& result) -> ScoreDocument {
  ScoreDocument document;
  document.backend = result.backend;
  document.comparable_across_workloads = false;
  document.note = "评分为单工作负载吞吐与相对 1 线程扩展指数；不同工作负载的操作单位不可直接相加";
  document.points.reserve(result.runs.size());
  for (std::size_t index = 0; index < result.runs.size(); ++index) {
    const auto& run = result.runs[index];
    const auto& scaling = result.thread_scaling[index];
    const auto operations_per_sample =
        static_cast<double>(run.operations) / static_cast<double>(result.config.repetitions);
    const auto throughput = run.skipped || run.statistics.median_ms <= 0.0
                                ? 0.0
                                : operations_per_sample * 1000.0 / run.statistics.median_ms;
    document.points.push_back(
        {.thread_count = run.thread_count,
         .throughput_operations_per_second = throughput,
         .thread_scaling_score = run.skipped ? 0.0 : scaling.speedup * 100.0});
  }
  return document;
}

} // namespace gamebench::benchmark
