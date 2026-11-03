#include "benchmark/vulkan_workload.hpp"
#include "benchmark/workload.hpp"

#include <gtest/gtest.h>

#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define GAMEBENCH_THREAD_SANITIZER 1
#endif
#endif

namespace {

[[nodiscard]] auto config(gamebench::core::VulkanMode mode)
    -> gamebench::benchmark::WorkloadConfig {
  return {
      .thread_count = 2,
      .seed = 42,
      .objects = 128,
      .iterations = 1,
      .scheduler = gamebench::core::SchedulerKind::WorkStealing,
      .layout = gamebench::core::DataLayout::SoA,
      .memory_pattern = gamebench::core::MemoryPattern::Sequential,
      .working_set_bytes = 0,
      .vulkan_mode = mode,
      .audio_buffer_size = 1024,
      .sample_rate = 48000,
  };
}

} // namespace

TEST(VulkanTest, SupportsNoSubmitOrReportsSkipped) {
#if defined(GAMEBENCH_THREAD_SANITIZER)
  GTEST_SKIP() << "RADV 驱动内部同步不被 ThreadSanitizer 识别";
#endif
  gamebench::benchmark::VulkanWorkload workload;
  const auto output = workload.execute(config(gamebench::core::VulkanMode::NoSubmit));
  if (output.skipped) {
    EXPECT_FALSE(output.validation_message.empty());
  } else {
    EXPECT_TRUE(output.valid);
    EXPECT_GT(output.operations, 0U);
    ASSERT_TRUE(output.vulkan.has_value());
    EXPECT_FALSE(output.vulkan->device_name.empty());
  }
}

TEST(VulkanTest, SupportsGpuBackedRecordsOrReportsSkipped) {
#if defined(GAMEBENCH_THREAD_SANITIZER)
  GTEST_SKIP() << "RADV 驱动内部同步不被 ThreadSanitizer 识别";
#endif
  gamebench::benchmark::VulkanWorkload workload;
  const auto output = workload.execute(config(gamebench::core::VulkanMode::GpuBacked));
  if (output.skipped) {
    EXPECT_FALSE(output.validation_message.empty());
  } else {
    EXPECT_TRUE(output.valid);
    EXPECT_GT(output.operations, 0U);
  }
}
