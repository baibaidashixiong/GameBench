#include "simulation/navigation.hpp"

#include <gtest/gtest.h>

#include <cstdlib>

TEST(NavigationTest, FindsWalkablePath) {
  gamebench::simulation::NavigationGrid grid(64, 64, 42);
  const auto path = grid.find_path({0, 0}, {63, 63});
  ASSERT_FALSE(path.empty());
  EXPECT_EQ(path.front(), (gamebench::simulation::GridCell{0, 0}));
  EXPECT_EQ(path.back(), (gamebench::simulation::GridCell{63, 63}));
  for (std::size_t index = 0; index < path.size(); ++index) {
    EXPECT_TRUE(grid.walkable(path[index]));
    if (index > 0) {
      const auto dx = std::abs(static_cast<int>(path[index].x) - path[index - 1].x);
      const auto dy = std::abs(static_cast<int>(path[index].y) - path[index - 1].y);
      EXPECT_EQ(dx + dy, 1);
    }
  }
}

TEST(NavigationTest, BehaviorTreeCoversDecisionBranches) {
  using gamebench::simulation::AgentAction;
  EXPECT_EQ(
      gamebench::simulation::evaluate_behavior(
          {.position_x = 0.0F, .position_y = 0.0F, .health = 10.0F, .distance_to_target = 1.0F}),
      AgentAction::Flee);
  EXPECT_EQ(
      gamebench::simulation::evaluate_behavior(
          {.position_x = 0.0F, .position_y = 0.0F, .health = 100.0F, .distance_to_target = 1.0F}),
      AgentAction::Attack);
  EXPECT_EQ(
      gamebench::simulation::evaluate_behavior(
          {.position_x = 0.0F, .position_y = 0.0F, .health = 100.0F, .distance_to_target = 10.0F}),
      AgentAction::Chase);
  EXPECT_EQ(
      gamebench::simulation::evaluate_behavior(
          {.position_x = 0.0F, .position_y = 0.0F, .health = 100.0F, .distance_to_target = 100.0F}),
      AgentAction::Idle);
}
