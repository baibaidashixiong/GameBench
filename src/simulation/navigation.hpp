#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace gamebench::simulation {

struct GridCell {
  std::uint16_t x{};
  std::uint16_t y{};

  friend auto operator==(const GridCell&, const GridCell&) -> bool = default;
};

class NavigationGrid final {
public:
  NavigationGrid(std::uint32_t width, std::uint32_t height, std::uint64_t seed);

  [[nodiscard]] auto find_path(GridCell start, GridCell goal) const -> std::vector<GridCell>;
  [[nodiscard]] auto walkable(GridCell cell) const -> bool;
  [[nodiscard]] auto width() const -> std::uint32_t;
  [[nodiscard]] auto height() const -> std::uint32_t;

private:
  [[nodiscard]] auto index(GridCell cell) const -> std::size_t;

  std::uint32_t width_{};
  std::uint32_t height_{};
  std::vector<bool> blocked_;
};

struct AgentState {
  float position_x{};
  float position_y{};
  float health{100.0F};
  float distance_to_target{};
};

enum class AgentAction : std::uint8_t { Idle, Chase, Attack, Flee };

[[nodiscard]] auto evaluate_behavior(const AgentState& state) -> AgentAction;

} // namespace gamebench::simulation
