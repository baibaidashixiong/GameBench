#include "platform/cpu_affinity.hpp"
#include "platform/cpu_topology.hpp"

#include <gtest/gtest.h>

#include <algorithm>

TEST(CpuTopologyTest, ReportsConsistentTopology) {
  const auto topology = gamebench::platform::detect_cpu_topology();
  EXPECT_GT(topology.logical_processors, 0U);
  EXPECT_GT(topology.physical_cores, 0U);
  EXPECT_LE(topology.physical_cores, topology.logical_processors);
  EXPECT_GE(topology.numa_nodes, 1U);
  EXPECT_FALSE(topology.allowed_cpus.empty());
  EXPECT_EQ(topology.ccd_type, "Unknown");
  EXPECT_EQ(topology.core_type, "Unknown");
}

TEST(CpuTopologyTest, ScopedAffinityRestoresCurrentMask) {
  const auto original = gamebench::platform::current_thread_affinity();
  ASSERT_FALSE(original.empty());
  {
    const unsigned cpu = original.front();
    gamebench::platform::ScopedThreadAffinity affinity(std::span{&cpu, 1});
    EXPECT_EQ(gamebench::platform::current_thread_affinity(), std::vector<unsigned>{cpu});
  }
  EXPECT_EQ(gamebench::platform::current_thread_affinity(), original);
}

#if defined(GAMEBENCH_HAS_HWLOC)
TEST(CpuTopologyTest, MapsPhysicalCoreAndNumaNodeToLogicalCpus) {
  const auto core_cpus = gamebench::platform::logical_cpus_for_physical_core(0);
  const auto node_cpus = gamebench::platform::logical_cpus_for_numa_node(0);
  EXPECT_FALSE(core_cpus.empty());
  EXPECT_FALSE(node_cpus.empty());
}
#endif
