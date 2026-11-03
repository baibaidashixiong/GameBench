#include "simulation/animation.hpp"

#include <gtest/gtest.h>

TEST(AnimationTest, SkeletonEvaluationIsDeterministic) {
  const auto first = gamebench::simulation::evaluate_skeleton(42, 32, 1.25F);
  const auto second = gamebench::simulation::evaluate_skeleton(42, 32, 1.25F);
  const auto later = gamebench::simulation::evaluate_skeleton(42, 32, 1.5F);
  EXPECT_EQ(first, second);
  EXPECT_NE(first, later);
  EXPECT_NE(first, 0U);
}
