#pragma once

#include "core/runner.hpp"

#include <string>
#include <vector>

namespace gamebench::benchmark {

struct ScorePoint {
  unsigned thread_count{};
  double throughput_operations_per_second{};
  double thread_scaling_score{};
};

struct ScoreDocument {
  std::string scoring_version{"1"};
  std::string backend;
  bool comparable_across_workloads{};
  std::string note;
  std::vector<ScorePoint> points;
};

[[nodiscard]] auto calculate_scores(const core::BenchmarkResult& result) -> ScoreDocument;

} // namespace gamebench::benchmark
