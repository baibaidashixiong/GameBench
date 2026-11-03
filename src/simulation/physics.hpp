#pragma once

#include "simulation/scene.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace gamebench::simulation {

struct RigidBody {
  Vec3 position;
  Vec3 velocity;
  float radius{};
  std::uint32_t id{};
};

struct CollisionStats {
  std::uint64_t broad_phase_pairs{};
  std::uint64_t contact_pairs{};
};

class PhysicsWorld final {
public:
  PhysicsWorld(std::size_t body_count, std::uint64_t seed);

  void integrate(std::size_t begin, std::size_t end, float delta_time);
  [[nodiscard]] auto collect_collisions() -> CollisionStats;
  [[nodiscard]] auto spatial_query(Vec3 center, float radius) const -> std::size_t;
  [[nodiscard]] auto checksum(std::size_t begin, std::size_t end) const -> std::uint64_t;
  [[nodiscard]] auto body_count() const -> std::size_t;

private:
  std::vector<RigidBody> bodies_;
};

} // namespace gamebench::simulation
