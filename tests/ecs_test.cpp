#include "core/config.hpp"
#include "simulation/ecs.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <vector>

TEST(EcsTest, LayoutsProduceIdenticalComponentChecksums) {
  constexpr std::array layouts{
      gamebench::core::DataLayout::AoS,
      gamebench::core::DataLayout::SoA,
      gamebench::core::DataLayout::AoSoA,
  };
  std::array<std::uint64_t, layouts.size()> checksums{};
  std::array<std::uint64_t, layouts.size()> random_checksums{};

  for (std::size_t layout_index = 0; layout_index < layouts.size(); ++layout_index) {
    gamebench::simulation::EcsWorld world(layouts[layout_index], 256);
    std::vector<gamebench::simulation::EntityId> entities;
    for (std::size_t index = 0; index < 256; ++index) {
      entities.push_back(world.create_entity());
    }

    world.update(0, 256, 0.016F);
    checksums[layout_index] = world.checksum(0, 256);
    random_checksums[layout_index] = world.random_access_checksum(42, 1024);
    EXPECT_EQ(world.alive_count(), 256U);
    EXPECT_TRUE(world.alive(entities.front()));

    world.destroy_every(4);
    EXPECT_EQ(world.alive_count(), 192U);
    EXPECT_FALSE(world.alive(entities[3]));
  }

  EXPECT_EQ(checksums[0], checksums[1]);
  EXPECT_EQ(checksums[1], checksums[2]);
  EXPECT_EQ(random_checksums[0], random_checksums[1]);
  EXPECT_EQ(random_checksums[1], random_checksums[2]);
}

TEST(EcsTest, ReusedEntityIndexIncrementsGeneration) {
  gamebench::simulation::EcsWorld world(gamebench::core::DataLayout::SoA, 4);
  const auto first = world.create_entity();
  world.destroy_entity(first);
  const auto replacement = world.create_entity();

  EXPECT_FALSE(world.alive(first));
  EXPECT_TRUE(world.alive(replacement));
  EXPECT_NE(replacement >> 32U, first >> 32U);
  EXPECT_EQ(world.alive_count(), 1U);
}
