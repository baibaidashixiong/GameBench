#include "simulation/scene.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
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

[[nodiscard]] auto dot(const Vec3& left, const Vec3& right) -> float {
  return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] auto center(const Aabb& bounds) -> Vec3 {
  return {(bounds.minimum.x + bounds.maximum.x) * 0.5F,
          (bounds.minimum.y + bounds.maximum.y) * 0.5F,
          (bounds.minimum.z + bounds.maximum.z) * 0.5F};
}

void expand(Aabb& bounds, const Aabb& other) {
  bounds.minimum.x = std::min(bounds.minimum.x, other.minimum.x);
  bounds.minimum.y = std::min(bounds.minimum.y, other.minimum.y);
  bounds.minimum.z = std::min(bounds.minimum.z, other.minimum.z);
  bounds.maximum.x = std::max(bounds.maximum.x, other.maximum.x);
  bounds.maximum.y = std::max(bounds.maximum.y, other.maximum.y);
  bounds.maximum.z = std::max(bounds.maximum.z, other.maximum.z);
}

[[nodiscard]] auto intersects(const Aabb& left, const Aabb& right) -> bool {
  return left.minimum.x <= right.maximum.x && left.maximum.x >= right.minimum.x &&
         left.minimum.y <= right.maximum.y && left.maximum.y >= right.minimum.y &&
         left.minimum.z <= right.maximum.z && left.maximum.z >= right.minimum.z;
}

[[nodiscard]] auto lod_for_distance(float distance_squared) -> std::uint8_t {
  if (distance_squared < 225.0F) {
    return 0;
  }
  if (distance_squared < 1600.0F) {
    return 1;
  }
  return 2;
}

} // namespace

auto Frustum::perspective(float near_distance, float far_distance, float half_fov_tangent)
    -> Frustum {
  Frustum result;
  result.planes[0] = {{1.0F, 0.0F, half_fov_tangent}, 0.0F};
  result.planes[1] = {{-1.0F, 0.0F, half_fov_tangent}, 0.0F};
  result.planes[2] = {{0.0F, 1.0F, half_fov_tangent}, 0.0F};
  result.planes[3] = {{0.0F, -1.0F, half_fov_tangent}, 0.0F};
  result.planes[4] = {{0.0F, 0.0F, 1.0F}, -near_distance};
  result.planes[5] = {{0.0F, 0.0F, -1.0F}, far_distance};
  return result;
}

auto Frustum::translated(Vec3 offset) const -> Frustum {
  Frustum result = *this;
  for (auto& plane : result.planes) {
    plane.distance -= dot(plane.normal, offset);
  }
  return result;
}

auto Frustum::intersects(const Aabb& bounds) const -> bool {
  for (const auto& plane : planes) {
    const Vec3 support{plane.normal.x >= 0.0F ? bounds.maximum.x : bounds.minimum.x,
                       plane.normal.y >= 0.0F ? bounds.maximum.y : bounds.minimum.y,
                       plane.normal.z >= 0.0F ? bounds.maximum.z : bounds.minimum.z};
    if (dot(plane.normal, support) + plane.distance < 0.0F) {
      return false;
    }
  }
  return true;
}

Scene::Scene(std::size_t object_count, std::uint64_t seed) {
  if (object_count == 0) {
    throw std::invalid_argument("场景对象数量必须大于零");
  }
  objects_.reserve(object_count);
  const auto width = static_cast<std::size_t>(std::ceil(std::sqrt(object_count)));
  for (std::size_t index = 0; index < object_count; ++index) {
    const auto state = mix64(seed + index);
    const auto column = index % width;
    const auto row = index / width;
    const auto jitter_x = static_cast<float>(state & 0xffffU) / 65535.0F - 0.5F;
    const auto jitter_y = static_cast<float>((state >> 16U) & 0xffffU) / 65535.0F - 0.5F;
    const auto jitter_z = static_cast<float>((state >> 32U) & 0xffffU) / 65535.0F - 0.5F;
    const auto cx =
        (static_cast<float>(column) - static_cast<float>(width) * 0.5F) * 2.5F + jitter_x * 1.5F;
    const auto cy =
        (static_cast<float>(row) - static_cast<float>(width) * 0.5F) * 2.5F + jitter_y * 1.5F;
    const auto cz = jitter_z * 8.0F + 30.0F;
    const auto half_x = 0.5F + static_cast<float>((state >> 48U) & 0xffU) / 512.0F;
    const auto half_y = 0.5F + static_cast<float>((state >> 56U) & 0xffU) / 512.0F;
    const auto half_z = 0.5F + static_cast<float>(state & 0xffU) / 512.0F;
    objects_.push_back({.bounds = {{cx - half_x, cy - half_y, cz - half_z},
                                   {cx + half_x, cy + half_y, cz + half_z}},
                        .mesh_index = static_cast<std::uint32_t>(state % 257U)});
  }

  object_order_.resize(objects_.size());
  std::iota(object_order_.begin(), object_order_.end(), 0U);
  nodes_.reserve(objects_.size() * 2U);
  build_bvh();
}

auto Scene::process_view(const Frustum& frustum, std::uint64_t query_seed) const
    -> SceneQueryResult {
  SceneQueryResult result;
  query_frustum_node(root_index_, frustum, result);
  result.visible_objects = result.draw_commands.size();

  const auto query_center_state = mix64(query_seed);
  const auto query_x = static_cast<float>(query_center_state & 0xffffU) / 65535.0F * 40.0F - 20.0F;
  const auto query_y =
      static_cast<float>((query_center_state >> 16U) & 0xffffU) / 65535.0F * 40.0F - 20.0F;
  const Aabb query_bounds{{query_x - 5.0F, query_y - 5.0F, 20.0F},
                          {query_x + 5.0F, query_y + 5.0F, 40.0F}};
  result.spatial_hits = query_aabb(query_bounds).size();

  result.checksum = mix64(query_seed ^ result.spatial_hits);
  for (const auto& draw : result.draw_commands) {
    const auto packed = static_cast<std::uint64_t>(draw.object_index) << 32U |
                        static_cast<std::uint64_t>(draw.mesh_index) << 8U | draw.lod;
    result.checksum = mix64(result.checksum ^ packed);
  }
  return result;
}

auto Scene::brute_force_visible_count(const Frustum& frustum) const -> std::size_t {
  return static_cast<std::size_t>(
      std::ranges::count_if(objects_, [&frustum](const SceneObject& object) {
        return frustum.intersects(object.bounds);
      }));
}

auto Scene::query_aabb(const Aabb& bounds) const -> std::vector<std::uint32_t> {
  std::vector<std::uint32_t> result;
  query_aabb_node(root_index_, bounds, result);
  return result;
}

auto Scene::object_count() const -> std::size_t {
  return objects_.size();
}

auto Scene::node_count() const -> std::size_t {
  return nodes_.size();
}

void Scene::build_bvh() {
  root_index_ = build_node(0, static_cast<std::uint32_t>(object_order_.size()));
}

auto Scene::build_node(std::uint32_t begin, std::uint32_t end) -> std::uint32_t {
  const auto node_index = static_cast<std::uint32_t>(nodes_.size());
  nodes_.push_back(BvhNode{});

  Aabb bounds = objects_[object_order_[begin]].bounds;
  const auto bounds_center = center(bounds);
  Aabb centroid_bounds{bounds_center, bounds_center};
  for (auto index = begin + 1; index < end; ++index) {
    const auto& object_bounds = objects_[object_order_[index]].bounds;
    expand(bounds, object_bounds);
    const auto object_center = center(object_bounds);
    expand(centroid_bounds, {object_center, object_center});
  }
  nodes_[node_index].bounds = bounds;

  const auto count = end - begin;
  if (count <= 4) {
    nodes_[node_index].leaf = true;
    nodes_[node_index].begin = begin;
    nodes_[node_index].count = count;
    return node_index;
  }

  const auto extent = Vec3{centroid_bounds.maximum.x - centroid_bounds.minimum.x,
                           centroid_bounds.maximum.y - centroid_bounds.minimum.y,
                           centroid_bounds.maximum.z - centroid_bounds.minimum.z};
  const auto axis =
      extent.x >= extent.y && extent.x >= extent.z ? 0 : (extent.y >= extent.z ? 1 : 2);
  auto middle = begin + count / 2U;
  std::nth_element(object_order_.begin() + begin, object_order_.begin() + middle,
                   object_order_.begin() + end,
                   [this, axis](std::uint32_t left, std::uint32_t right) {
                     const auto left_center = center(objects_[left].bounds);
                     const auto right_center = center(objects_[right].bounds);
                     if (axis == 0) {
                       return left_center.x < right_center.x;
                     }
                     if (axis == 1) {
                       return left_center.y < right_center.y;
                     }
                     return left_center.z < right_center.z;
                   });

  const auto left = build_node(begin, middle);
  const auto right = build_node(middle, end);
  nodes_[node_index].left = left;
  nodes_[node_index].right = right;
  return node_index;
}

void Scene::query_aabb_node(std::uint32_t node_index, const Aabb& bounds,
                            std::vector<std::uint32_t>& output) const {
  const auto& node = nodes_[node_index];
  if (!intersects(node.bounds, bounds)) {
    return;
  }
  if (node.leaf) {
    for (auto index = node.begin; index < node.begin + node.count; ++index) {
      const auto object_index = object_order_[index];
      if (intersects(objects_[object_index].bounds, bounds)) {
        output.push_back(object_index);
      }
    }
    return;
  }
  query_aabb_node(node.left, bounds, output);
  query_aabb_node(node.right, bounds, output);
}

void Scene::query_frustum_node(std::uint32_t node_index, const Frustum& frustum,
                               SceneQueryResult& result) const {
  const auto& node = nodes_[node_index];
  if (!frustum.intersects(node.bounds)) {
    return;
  }
  if (node.leaf) {
    for (auto index = node.begin; index < node.begin + node.count; ++index) {
      const auto object_index = object_order_[index];
      const auto& object = objects_[object_index];
      if (!frustum.intersects(object.bounds)) {
        continue;
      }
      const auto object_center = center(object.bounds);
      const auto dx = object_center.x;
      const auto dy = object_center.y;
      const auto dz = object_center.z;
      result.draw_commands.push_back({.object_index = object_index,
                                      .mesh_index = object.mesh_index,
                                      .lod = lod_for_distance(dx * dx + dy * dy + dz * dz)});
    }
    return;
  }
  query_frustum_node(node.left, frustum, result);
  query_frustum_node(node.right, frustum, result);
}

} // namespace gamebench::simulation
