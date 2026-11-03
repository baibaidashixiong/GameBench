#include "benchmark/engine_adapter.hpp"

#include <gtest/gtest.h>

#include <cstdlib>

TEST(EngineAdapterTest, ListsGodotSource2AndPlannedEngines) {
  const auto adapters = gamebench::benchmark::planned_engine_adapters();
  ASSERT_EQ(adapters.size(), 4U);
  EXPECT_EQ(adapters[0].id, "godot");
  EXPECT_EQ(adapters[1].id, "source2");
  EXPECT_EQ(adapters[2].id, "unreal");
  EXPECT_EQ(adapters[3].id, "o3de");
  EXPECT_EQ(adapters[2].availability, gamebench::benchmark::EngineAvailability::NotImplemented);
  EXPECT_FALSE(adapters[0].message.empty());
  EXPECT_FALSE(adapters[1].message.empty());
}

TEST(EngineAdapterTest, DetectsExplicitSource2BinaryWithoutClaimingRunnable) {
  unsetenv("SOURCE2_ROOT");
  setenv("SOURCE2_BIN", "/bin/true", 1);
  const auto adapter = gamebench::benchmark::detect_source2_adapter();
  unsetenv("SOURCE2_BIN");

  EXPECT_EQ(adapter.id, "source2");
  EXPECT_EQ(adapter.availability, gamebench::benchmark::EngineAvailability::DetectedOnly);
  EXPECT_NE(adapter.message.find("执行桥接尚未实现"), std::string::npos);
}
