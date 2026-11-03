#include "benchmark/registry.hpp"
#include "benchmark/workload.hpp"

#include <gtest/gtest.h>

#include <set>

TEST(CompositeFrameTest, Source2LikeFrameValidatesAndReportsScheduling) {
  auto workload = gamebench::benchmark::WorkloadRegistry::create_default().create("game-frame");
  ASSERT_NE(workload, nullptr);
  const gamebench::benchmark::WorkloadConfig config{
      .profile_id = "source2-like",
      .thread_count = 2,
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

  const auto output = workload->execute(config);
  EXPECT_TRUE(output.valid);
  EXPECT_GT(output.checksum, 0U);
  EXPECT_GT(output.operations, 0U);
  ASSERT_TRUE(output.scheduling.has_value());
  EXPECT_EQ(output.scheduling->submitted_jobs, output.scheduling->completed_jobs);
  EXPECT_GT(output.scheduling->submitted_jobs, 0U);
  std::set<double> load_levels;
  ASSERT_TRUE(output.simulated_load_percent.has_value());
  load_levels.insert(*output.simulated_load_percent);

  const auto next_output = workload->execute(config);
  ASSERT_TRUE(next_output.scheduling.has_value());
  ASSERT_TRUE(next_output.simulated_load_percent.has_value());
  load_levels.insert(*next_output.simulated_load_percent);
  EXPECT_EQ(next_output.scheduling->submitted_jobs, next_output.scheduling->completed_jobs);
  for (unsigned frame = 0; frame < 10; ++frame) {
    const auto repeated = workload->execute(config);
    ASSERT_TRUE(repeated.scheduling.has_value());
    ASSERT_TRUE(repeated.simulated_load_percent.has_value());
    load_levels.insert(*repeated.simulated_load_percent);
    EXPECT_EQ(repeated.scheduling->submitted_jobs, repeated.scheduling->completed_jobs)
        << "frame " << frame;
    EXPECT_LT(repeated.scheduling->submitted_jobs, 100U) << "frame " << frame;
  }
  EXPECT_GE(load_levels.size(), 2U);
}
