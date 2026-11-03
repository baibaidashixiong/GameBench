#include "benchmark/frame_profile.hpp"

#include <gtest/gtest.h>

TEST(FrameProfileTest, Source2LikeProfileIsVersionedAndComplete) {
  const auto profile = gamebench::benchmark::find_frame_profile("source2-like");
  ASSERT_TRUE(profile.has_value());
  EXPECT_EQ(profile->id, "source2-like");
  EXPECT_EQ(profile->version, "2");
  EXPECT_EQ(profile->load_variation_version, 1);
  EXPECT_LT(profile->minimum_load_percent, profile->maximum_load_percent);
  EXPECT_GT(profile->ecs_entities, 0U);
  EXPECT_GT(profile->scene_objects, 0U);
  EXPECT_GT(profile->physics_bodies, 0U);
  EXPECT_GT(profile->animation_skeletons, 0U);
  EXPECT_GT(profile->ai_agents, 0U);
  EXPECT_GT(profile->audio_sources, 0U);
  EXPECT_GT(profile->vulkan_tasks, 0U);
}

TEST(FrameProfileTest, RejectsUnknownProfile) {
  EXPECT_FALSE(gamebench::benchmark::find_frame_profile("unknown").has_value());
}
