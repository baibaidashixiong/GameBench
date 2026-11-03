#include "benchmark/registry.hpp"
#include "benchmark/workload.hpp"

#include <gtest/gtest.h>

TEST(WorkloadTest, IntegerWorkloadIsDeterministic) {
  auto workload = gamebench::benchmark::WorkloadRegistry::create_default().create("integer");
  ASSERT_NE(workload, nullptr);

  const gamebench::benchmark::WorkloadConfig config{
      .thread_count = 1, .seed = 42, .objects = 4096, .iterations = 1};
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
  EXPECT_EQ(registry.descriptors().size(), 1U);
}

