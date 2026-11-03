#include "benchmark/registry.hpp"
#include "core/config.hpp"
#include "core/result_manager.hpp"
#include "core/runner.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

TEST(RunnerTest, RunsAndValidatesWorkload) {
  auto workload = gamebench::benchmark::WorkloadRegistry::create_default().create("integer");
  ASSERT_NE(workload, nullptr);

  gamebench::core::RunConfig config;
  config.thread_counts = {1};
  config.objects = 1024;
  config.warmup_iterations = 1;
  config.repetitions = 3;

  const auto result = gamebench::core::BenchmarkRunner{}.run(*workload, config);

  EXPECT_TRUE(result.valid);
  ASSERT_EQ(result.runs.size(), 1U);
  EXPECT_EQ(result.runs.front().samples_ms.size(), 3U);
  EXPECT_TRUE(result.runs.front().valid);
  EXPECT_EQ(result.runs.front().operations, 3072U);
  EXPECT_DOUBLE_EQ(result.thread_scaling.front().speedup, 1.0);
}

TEST(ResultManagerTest, WritesMachineReadableResults) {
  auto workload = gamebench::benchmark::WorkloadRegistry::create_default().create("integer");
  ASSERT_NE(workload, nullptr);

  gamebench::core::RunConfig config;
  config.thread_counts = {1};
  config.objects = 32;
  config.warmup_iterations = 0;
  config.repetitions = 1;
  const auto result = gamebench::core::BenchmarkRunner{}.run(*workload, config);

  const auto path = std::filesystem::temp_directory_path() / "gamebench-result-test.json";
  gamebench::core::ResultManager::write(result, path, gamebench::core::ExportFormat::Json);

  std::ifstream input(path);
  const std::string contents{std::istreambuf_iterator<char>(input),
                             std::istreambuf_iterator<char>()};
  EXPECT_NE(contents.find("\"benchmark_id\": \"integer\""), std::string::npos);
  EXPECT_NE(contents.find("\"validation_status\": \"passed\""), std::string::npos);
  EXPECT_NE(contents.find("\"thread_scaling\""), std::string::npos);
  std::filesystem::remove(path);
}
