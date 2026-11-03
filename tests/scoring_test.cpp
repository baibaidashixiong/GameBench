#include "benchmark/scoring.hpp"
#include "core/runner.hpp"

#include <gtest/gtest.h>

TEST(ScoringTest, ReportsTransparentThroughputAndScaling) {
  gamebench::core::BenchmarkResult result;
  result.backend = "synthetic";
  result.config.repetitions = 2;
  gamebench::core::BenchmarkRun run;
  run.thread_count = 2;
  run.operations = 2000;
  run.statistics.median_ms = 10.0;
  run.valid = true;
  result.runs.push_back(run);
  result.thread_scaling.push_back({.thread_count = 2, .speedup = 1.5, .efficiency = 0.75});

  const auto scores = gamebench::benchmark::calculate_scores(result);
  ASSERT_EQ(scores.points.size(), 1U);
  EXPECT_DOUBLE_EQ(scores.points[0].throughput_operations_per_second, 100000.0);
  EXPECT_DOUBLE_EQ(scores.points[0].thread_scaling_score, 150.0);
  EXPECT_FALSE(scores.comparable_across_workloads);
}
