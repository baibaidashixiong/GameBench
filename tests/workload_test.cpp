#include "benchmark/registry.hpp"
#include "benchmark/workload.hpp"

#include <gtest/gtest.h>

TEST(WorkloadTest, IntegerWorkloadIsDeterministic) {
  auto workload = gamebench::benchmark::WorkloadRegistry::create_default().create("integer");
  ASSERT_NE(workload, nullptr);

  const gamebench::benchmark::WorkloadConfig config{
      .thread_count = 1,
      .seed = 42,
      .objects = 4096,
      .iterations = 1,
      .scheduler = gamebench::core::SchedulerKind::WorkStealing};
  const auto first = workload->execute(config);
  const auto second = workload->execute(config);

  EXPECT_TRUE(first.valid);
  EXPECT_EQ(first.checksum, second.checksum);
  EXPECT_EQ(first.operations, 4096U);
}

TEST(WorkloadTest, RegistryListsRegisteredWorkloads) {
  const auto registry = gamebench::benchmark::WorkloadRegistry::create_default();
  EXPECT_TRUE(registry.contains("integer"));
  EXPECT_FALSE(registry.contains("missing"));
  EXPECT_EQ(registry.descriptors().size(), 2U);
}

TEST(WorkloadTest, SchedulerWorkloadIsDeterministicAcrossModes) {
  auto workload = gamebench::benchmark::WorkloadRegistry::create_default().create("scheduler");
  ASSERT_NE(workload, nullptr);

  constexpr auto seeds = std::array{0x1234ULL, 0x1234ULL, 0x1234ULL};
  constexpr auto modes = std::array{
      gamebench::core::SchedulerKind::GlobalQueue,
      gamebench::core::SchedulerKind::StaticPartitioning,
      gamebench::core::SchedulerKind::WorkStealing,
  };

  std::array<std::uint64_t, 3> checksums{};
  for (std::size_t index = 0; index < modes.size(); ++index) {
    const gamebench::benchmark::WorkloadConfig config{
        .thread_count = 2,
        .seed = seeds[index],
        .objects = 8192,
        .iterations = 1,
        .scheduler = modes[index],
    };
    const auto output = workload->execute(config);
    EXPECT_TRUE(output.valid);
    EXPECT_TRUE(output.scheduling.has_value());
    checksums[index] = output.checksum;
  }

  EXPECT_EQ(checksums[0], checksums[1]);
  EXPECT_EQ(checksums[1], checksums[2]);
}
