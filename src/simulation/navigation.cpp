#include "simulation/navigation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace gamebench::simulation {
namespace {

[[nodiscard]] constexpr auto mix64(std::uint64_t value) -> std::uint64_t {
  value ^= value >> 30U;
  value *= 0xbf58476d1ce4e5b9ULL;
  value ^= value >> 27U;
  value *= 0x94d049bb133111ebULL;
  value ^= value >> 31U;
  return value;
}

[[nodiscard]] auto heuristic(GridCell left, GridCell right) -> std::uint32_t {
  const auto dx = static_cast<std::uint32_t>(std::max(left.x, right.x) - std::min(left.x, right.x));
  const auto dy = static_cast<std::uint32_t>(std::max(left.y, right.y) - std::min(left.y, right.y));
  return dx + dy;
}

} // namespace

NavigationGrid::NavigationGrid(std::uint32_t width, std::uint32_t height, std::uint64_t seed)
    : width_(width), height_(height), blocked_(static_cast<std::size_t>(width) * height) {
  if (width < 2 || height < 2) {
    throw std::invalid_argument("导航网格尺寸至少为 2x2");
  }
  for (std::uint32_t y = 0; y < height_; ++y) {
    for (std::uint32_t x = 0; x < width_; ++x) {
      const bool border_corridor = y == 0 || x == width_ - 1;
      const auto state = mix64(seed + static_cast<std::uint64_t>(y) * width_ + x);
      blocked_[index({static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y)})] =
          !border_corridor && (state % 100U) < 18U;
    }
  }
}

auto NavigationGrid::find_path(GridCell start, GridCell goal) const -> std::vector<GridCell> {
  if (!walkable(start) || !walkable(goal)) {
    return {};
  }
  struct OpenEntry {
    std::uint32_t score{};
    std::size_t index{};
  };
  const auto compare = [](const OpenEntry& left, const OpenEntry& right) {
    return left.score > right.score;
  };
  std::priority_queue<OpenEntry, std::vector<OpenEntry>, decltype(compare)> open(compare);
  const auto cell_count = blocked_.size();
  std::vector<std::uint32_t> cost(cell_count, std::numeric_limits<std::uint32_t>::max());
  std::vector<std::size_t> parent(cell_count, std::numeric_limits<std::size_t>::max());
  const auto start_index = index(start);
  const auto goal_index = index(goal);
  cost[start_index] = 0;
  open.push({heuristic(start, goal), start_index});
  constexpr std::array<std::pair<int, int>, 4> directions{std::pair{1, 0}, std::pair{-1, 0},
                                                          std::pair{0, 1}, std::pair{0, -1}};

  while (!open.empty()) {
    const auto current = open.top();
    open.pop();
    if (current.index == goal_index) {
      break;
    }
    const auto x = static_cast<std::uint32_t>(current.index % width_);
    const auto y = static_cast<std::uint32_t>(current.index / width_);
    for (const auto& [dx, dy] : directions) {
      const auto nx = static_cast<int>(x) + dx;
      const auto ny = static_cast<int>(y) + dy;
      if (nx < 0 || ny < 0 || nx >= static_cast<int>(width_) || ny >= static_cast<int>(height_)) {
        continue;
      }
      const GridCell neighbor{static_cast<std::uint16_t>(nx), static_cast<std::uint16_t>(ny)};
      if (!walkable(neighbor)) {
        continue;
      }
      const auto neighbor_index = index(neighbor);
      const auto next_cost = cost[current.index] + 1U;
      if (next_cost >= cost[neighbor_index]) {
        continue;
      }
      cost[neighbor_index] = next_cost;
      parent[neighbor_index] = current.index;
      open.push({next_cost + heuristic(neighbor, goal), neighbor_index});
    }
  }

  if (parent[goal_index] == std::numeric_limits<std::size_t>::max() && goal_index != start_index) {
    return {};
  }
  std::vector<GridCell> path;
  auto current = goal_index;
  while (true) {
    path.push_back({static_cast<std::uint16_t>(current % width_),
                    static_cast<std::uint16_t>(current / width_)});
    if (current == start_index) {
      break;
    }
    current = parent[current];
  }
  std::ranges::reverse(path);
  return path;
}

auto NavigationGrid::walkable(GridCell cell) const -> bool {
  return cell.x < width_ && cell.y < height_ &&
         !blocked_[static_cast<std::size_t>(cell.y) * width_ + cell.x];
}

auto NavigationGrid::width() const -> std::uint32_t {
  return width_;
}
auto NavigationGrid::height() const -> std::uint32_t {
  return height_;
}

auto NavigationGrid::index(GridCell cell) const -> std::size_t {
  return static_cast<std::size_t>(cell.y) * width_ + cell.x;
}

auto evaluate_behavior(const AgentState& state) -> AgentAction {
  if (state.health < 25.0F) {
    return AgentAction::Flee;
  }
  if (state.distance_to_target < 2.0F) {
    return AgentAction::Attack;
  }
  if (state.distance_to_target < 40.0F) {
    return AgentAction::Chase;
  }
  return AgentAction::Idle;
}

} // namespace gamebench::simulation
