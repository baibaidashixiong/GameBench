#include "benchmark/process_mix_workload.hpp"
#include "core/config.hpp"
#include "core/result_manager.hpp"
#include "core/runner.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {

[[nodiscard]] auto test_config(unsigned thread_count) -> gamebench::benchmark::WorkloadConfig {
  return {
      .thread_count = thread_count,
      .seed = 42,
      .objects = 0,
      .iterations = 1,
      .scheduler = gamebench::core::SchedulerKind::WorkStealing,
      .layout = gamebench::core::DataLayout::SoA,
      .memory_pattern = gamebench::core::MemoryPattern::Sequential,
      .working_set_bytes = 0,
      .vulkan_mode = gamebench::core::VulkanMode::NoSubmit,
      .audio_buffer_size = 512,
      .sample_rate = 48000,
      .cpu_affinity = {},
      .enable_vulkan = false,
  };
}

} // namespace

TEST(ProcessMixTest, SpawnsOneMainGameAndFourAuxiliaryProcesses) {
  gamebench::benchmark::ProcessMixWorkload workload{
      std::filesystem::path{GAMEBENCH_TEST_EXECUTABLE}};
  const auto output = workload.execute(test_config(5));

  ASSERT_TRUE(output.valid) << output.validation_message;
  ASSERT_TRUE(output.process.has_value());
  EXPECT_EQ(output.execution_mode, "main-game-plus-aux-processes");
  EXPECT_EQ(output.process->coordinator_processes, 1U);
  EXPECT_EQ(output.process->game_processes, 5U);
  EXPECT_EQ(output.process->main_game_processes, 1U);
  EXPECT_EQ(output.process->auxiliary_processes, 4U);
  EXPECT_EQ(output.process->total_processes, 6U);
  EXPECT_EQ(output.process->main_thread_count, 1U);
  EXPECT_EQ(output.process->auxiliary_thread_count, 4U);
  EXPECT_EQ(output.process->total_thread_count, 5U);
  EXPECT_GT(output.operations, 0U);
  EXPECT_GT(output.process->child_cpu_ms, 0.0);
  EXPECT_GT(output.process->peak_rss_kb, 0.0);
  EXPECT_FALSE(output.simd_backend.empty());
}

TEST(ProcessMixTest, SameSeedProducesSameFirstFrameChecksum) {
  gamebench::benchmark::ProcessMixWorkload first{std::filesystem::path{GAMEBENCH_TEST_EXECUTABLE}};
  gamebench::benchmark::ProcessMixWorkload second{std::filesystem::path{GAMEBENCH_TEST_EXECUTABLE}};

  const auto first_output = first.execute(test_config(5));
  const auto second_output = second.execute(test_config(5));

  ASSERT_TRUE(first_output.valid) << first_output.validation_message;
  ASSERT_TRUE(second_output.valid) << second_output.validation_message;
  EXPECT_EQ(first_output.checksum, second_output.checksum);
  EXPECT_EQ(first_output.operations, second_output.operations);
}

TEST(ProcessMixTest, RejectsTopologyBelowFiveProcesses) {
  gamebench::benchmark::ProcessMixWorkload workload{
      std::filesystem::path{GAMEBENCH_TEST_EXECUTABLE}};
  EXPECT_THROW((void)workload.execute(test_config(4)), std::invalid_argument);
}

TEST(ProcessMixTest, RunnerAggregatesProcessMetricsAcrossSamples) {
  gamebench::benchmark::ProcessMixWorkload workload{
      std::filesystem::path{GAMEBENCH_TEST_EXECUTABLE}};
  gamebench::core::RunConfig config;
  config.workload = "game-process-mix";
  config.thread_counts = {5};
  config.objects = 1;
  config.warmup_iterations = 0;
  config.repetitions = 2;

  const auto result = gamebench::core::BenchmarkRunner{}.run(workload, config);

  ASSERT_TRUE(result.valid);
  ASSERT_EQ(result.runs.size(), 1U);
  const auto& run = result.runs.front();
  ASSERT_TRUE(run.process.has_value());
  EXPECT_EQ(run.process->game_processes, 5U);
  EXPECT_EQ(run.process->total_processes, 6U);
  EXPECT_EQ(run.process->main_game_processes, 1U);
  EXPECT_EQ(run.process->auxiliary_processes, 4U);
  EXPECT_GT(run.process->coordinator_cpu_ms, 0.0);
  EXPECT_GT(run.process->main_process_cpu_ms, 0.0);
  EXPECT_GT(run.process->auxiliary_cpu_ms, 0.0);
  ASSERT_TRUE(run.total_cpu_work_ms.has_value());
  EXPECT_GT(*run.total_cpu_work_ms, run.process->child_cpu_ms);
}

TEST(ProcessMixTest, JsonReportsMultiProcessExecutionDomainAndTopology) {
  gamebench::benchmark::ProcessMixWorkload workload{
      std::filesystem::path{GAMEBENCH_TEST_EXECUTABLE}};
  gamebench::core::RunConfig config;
  config.workload = "game-process-mix";
  config.thread_counts = {5};
  config.objects = 1;
  config.warmup_iterations = 0;
  config.repetitions = 1;
  const auto result = gamebench::core::BenchmarkRunner{}.run(workload, config);
  ASSERT_TRUE(result.valid);

  const auto path = std::filesystem::temp_directory_path() / "gamebench-process-mix-test.json";
  gamebench::core::ResultManager::write(result, path, gamebench::core::ExportFormat::Json);
  std::ifstream input(path);
  const std::string contents{std::istreambuf_iterator<char>(input),
                             std::istreambuf_iterator<char>()};
  EXPECT_NE(contents.find("\"execution_domain\": \"multi-process\""), std::string::npos);
  EXPECT_NE(contents.find("\"game_processes\": 5"), std::string::npos);
  EXPECT_NE(contents.find("\"total_processes\": 6"), std::string::npos);
  EXPECT_NE(contents.find("\"thread_scaling_reference\": 5"), std::string::npos);
  std::filesystem::remove(path);
}
