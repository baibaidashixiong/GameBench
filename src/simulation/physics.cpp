#include "simulation/physics.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <numeric>
#include <stdexcept>

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

[[nodiscard]] auto distance_squared(const Vec3& left, const Vec3& right) -> float {
  const auto dx = left.x - right.x;
  const auto dy = left.y - right.y;
  const auto dz = left.z - right.z;
  return dx * dx + dy * dy + dz * dz;
}

} // namespace

PhysicsWorld::PhysicsWorld(std::size_t body_count, std::uint64_t seed) {
  if (body_count == 0) {
    throw std::invalid_argument("物理刚体数量必须大于零");
  }
  bodies_.reserve(body_count);
  const auto width = static_cast<std::size_t>(std::ceil(std::sqrt(body_count)));
  for (std::size_t index = 0; index < body_count; ++index) {
    const auto state = mix64(seed + index);
    const auto x = static_cast<float>(index % width) * 0.9F;
    const auto y = static_cast<float>(index / width) * 0.9F;
    bodies_.push_back({.position = {x, y, static_cast<float>(state & 0xffU) / 255.0F},
                       .velocity = {0.25F + static_cast<float>(state & 7U) / 64.0F, -0.125F,
                                    static_cast<float>((state >> 8U) & 7U) / 128.0F},
                       .radius = 0.4F + static_cast<float>((state >> 16U) & 0xffU) / 1024.0F,
                       .id = static_cast<std::uint32_t>(index)});
  }
}

void PhysicsWorld::integrate(std::size_t begin, std::size_t end, float delta_time) {
  end = std::min(end, bodies_.size());
  for (auto index = begin; index < end; ++index) {
    auto& body = bodies_[index];
    body.position.x += body.velocity.x * delta_time;
    body.position.y += body.velocity.y * delta_time;
    body.position.z += body.velocity.z * delta_time;
    body.velocity.y -= 9.81F * delta_time;
  }
}

auto PhysicsWorld::collect_collisions() -> CollisionStats {
  std::vector<std::uint32_t> order(bodies_.size());
  std::iota(order.begin(), order.end(), 0U);
  std::ranges::sort(order, [this](std::uint32_t left, std::uint32_t right) {
    return bodies_[left].position.x - bodies_[left].radius <
           bodies_[right].position.x - bodies_[right].radius;
  });

  CollisionStats stats;
  for (std::size_t left_index = 0; left_index < order.size(); ++left_index) {
    const auto& left = bodies_[order[left_index]];
    const auto left_max = left.position.x + left.radius;
    for (auto right_index = left_index + 1; right_index < order.size(); ++right_index) {
      const auto& right = bodies_[order[right_index]];
      if (right.position.x - right.radius > left_max) {
        break;
      }
      if (std::abs(right.position.y - left.position.y) > left.radius + right.radius ||
          std::abs(right.position.z - left.position.z) > left.radius + right.radius) {
        continue;
      }
      ++stats.broad_phase_pairs;
      const auto combined_radius = left.radius + right.radius;
      if (distance_squared(left.position, right.position) <= combined_radius * combined_radius) {
        ++stats.contact_pairs;
      }
    }
  }
  return stats;
}

auto PhysicsWorld::spatial_query(Vec3 center, float radius) const -> std::size_t {
  const auto radius_squared = radius * radius;
  return static_cast<std::size_t>(std::ranges::count_if(bodies_, [&](const RigidBody& body) {
    return distance_squared(body.position, center) <= radius_squared;
  }));
}

auto PhysicsWorld::checksum(std::size_t begin, std::size_t end) const -> std::uint64_t {
  end = std::min(end, bodies_.size());
  std::uint64_t checksum = 0x9e3779b97f4a7c15ULL;
  for (auto index = begin; index < end; ++index) {
    const auto& body = bodies_[index];
    checksum = mix64(checksum ^ std::bit_cast<std::uint32_t>(body.position.x));
    checksum = mix64(checksum ^ std::bit_cast<std::uint32_t>(body.position.y));
    checksum = mix64(checksum ^ body.id);
  }
  return checksum;
}

auto PhysicsWorld::body_count() const -> std::size_t {
  return bodies_.size();
}

} // namespace gamebench::simulation
