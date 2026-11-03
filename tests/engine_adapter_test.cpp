#include "benchmark/engine_adapter.hpp"

#include <gtest/gtest.h>

TEST(EngineAdapterTest, ListsGodotAndPlannedEngines) {
  const auto adapters = gamebench::benchmark::planned_engine_adapters();
  ASSERT_EQ(adapters.size(), 3U);
  EXPECT_EQ(adapters[0].id, "godot");
  EXPECT_EQ(adapters[1].id, "unreal");
  EXPECT_EQ(adapters[2].id, "o3de");
  EXPECT_EQ(adapters[1].availability, gamebench::benchmark::EngineAvailability::NotImplemented);
  EXPECT_FALSE(adapters[0].message.empty());
}
