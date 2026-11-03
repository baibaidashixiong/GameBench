#include "benchmark/frame_profile.hpp"
#include "benchmark/registry.hpp"
#include "benchmark/workload.hpp"

#include <gtest/gtest.h>

#include <string_view>

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
  const auto profile = gamebench::benchmark::find_frame_profile("source2-like");
  ASSERT_TRUE(profile.has_value());
  const auto static_operations =
      profile->ecs_entities + profile->physics_bodies * profile->physics_substeps +
      profile->animation_skeletons * profile->bones_per_skeleton + profile->ai_agents +
      profile->scene_objects * profile->scene_sets * profile->scene_views +
      profile->simd_particles * profile->simd_steps +
      profile->audio_sources * profile->audio_buffer_size * 8U;
  EXPECT_GT(output.operations, static_operations);
  EXPECT_FALSE(std::string_view{output.simd_backend}.empty());
  EXPECT_EQ(output.worker_count, 1U);
  EXPECT_EQ(output.execution_mode, "main-plus-workers");
  ASSERT_TRUE(output.scheduling.has_value());
  EXPECT_EQ(output.scheduling->submitted_jobs, output.scheduling->completed_jobs);
  EXPECT_GT(output.scheduling->submitted_jobs, 0U);

  const auto next_output = workload->execute(config);
  ASSERT_TRUE(next_output.scheduling.has_value());
  EXPECT_EQ(next_output.scheduling->submitted_jobs, output.scheduling->submitted_jobs);
  EXPECT_EQ(next_output.scheduling->completed_jobs, output.scheduling->completed_jobs);
  auto expected_jobs = next_output.scheduling->submitted_jobs;
  for (unsigned frame = 0; frame < 10; ++frame) {
    const auto repeated = workload->execute(config);
    ASSERT_TRUE(repeated.scheduling.has_value());
    EXPECT_EQ(repeated.scheduling->submitted_jobs, expected_jobs) << "frame " << frame;
    EXPECT_EQ(repeated.scheduling->completed_jobs, expected_jobs) << "frame " << frame;
  }
}

TEST(CompositeFrameTest, SingleThreadIsMainInline) {
  auto workload = gamebench::benchmark::WorkloadRegistry::create_default().create("game-frame");
  ASSERT_NE(workload, nullptr);
  const gamebench::benchmark::WorkloadConfig config{
      .profile_id = "source2-like",
      .thread_count = 1,
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
  ASSERT_TRUE(output.valid);
  ASSERT_TRUE(output.scheduling.has_value());
  EXPECT_EQ(output.worker_count, 0U);
  EXPECT_EQ(output.execution_mode, "main-inline");
  EXPECT_EQ(output.scheduling->main_thread_jobs, output.scheduling->submitted_jobs);
  EXPECT_EQ(output.scheduling->worker_busy_ms, 0.0);
}
