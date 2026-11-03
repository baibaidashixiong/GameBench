#include "simulation/physics.hpp"

#include <gtest/gtest.h>

TEST(PhysicsTest, IntegratesAndCollectsValidCollisionStats) {
  gamebench::simulation::PhysicsWorld world(4096, 42);
  for (unsigned step = 0; step < 4; ++step) {
    world.integrate(0, world.body_count(), 1.0F / 120.0F);
  }
  const auto collisions = world.collect_collisions();
  EXPECT_LE(collisions.contact_pairs, collisions.broad_phase_pairs);
  EXPECT_GT(collisions.broad_phase_pairs, 0U);
  EXPECT_GT(world.spatial_query({20.0F, 20.0F, 1.0F}, 12.0F), 0U);
}

TEST(PhysicsTest, ChecksumIsDeterministic) {
  gamebench::simulation::PhysicsWorld first(1024, 7);
  gamebench::simulation::PhysicsWorld second(1024, 7);
  first.integrate(0, first.body_count(), 0.01F);
  second.integrate(0, second.body_count(), 0.01F);
  EXPECT_EQ(first.checksum(0, first.body_count()), second.checksum(0, second.body_count()));
}
