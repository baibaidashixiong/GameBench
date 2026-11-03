#include "simulation/scene.hpp"

#include <gtest/gtest.h>

TEST(SceneTest, BvhVisibilityMatchesBruteForce) {
  gamebench::simulation::Scene scene(4096, 42);
  const auto base = gamebench::simulation::Frustum::perspective(1.0F, 100.0F, 0.7002075F);

  for (int x = -20; x <= 20; x += 10) {
    const auto frustum = base.translated({static_cast<float>(x), 0.0F, 0.0F});
    const auto query = scene.process_view(frustum, 99);
    EXPECT_EQ(query.visible_objects, scene.brute_force_visible_count(frustum));
    EXPECT_EQ(query.visible_objects, query.draw_commands.size());
    EXPECT_GT(query.spatial_hits, 0U);
  }
  EXPECT_LT(scene.node_count(), scene.object_count() * 2U);
}

TEST(SceneTest, SpatialAabbQueryReturnsIntersectingObjects) {
  gamebench::simulation::Scene scene(1024, 7);
  const auto hits = scene.query_aabb({{-5.0F, -5.0F, 20.0F}, {5.0F, 5.0F, 40.0F}});
  EXPECT_FALSE(hits.empty());
  for (const auto index : hits) {
    EXPECT_LT(index, scene.object_count());
  }
}
