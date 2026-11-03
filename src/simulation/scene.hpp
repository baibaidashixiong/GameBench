#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace gamebench::simulation {

struct Vec3 {
  float x{};
  float y{};
  float z{};
};

struct Aabb {
  Vec3 minimum;
  Vec3 maximum;
};

struct Plane {
  Vec3 normal;
  float distance{};
};

struct Frustum {
  Plane planes[6];

  [[nodiscard]] static auto perspective(float near_distance, float far_distance,
                                        float half_fov_tangent) -> Frustum;
  [[nodiscard]] auto translated(Vec3 offset) const -> Frustum;
  [[nodiscard]] auto intersects(const Aabb& bounds) const -> bool;
};

struct DrawCommand {
  std::uint32_t object_index{};
  std::uint32_t mesh_index{};
  std::uint8_t lod{};
};

struct SceneQueryResult {
  std::uint64_t visible_objects{};
  std::uint64_t spatial_hits{};
  std::uint64_t checksum{};
  std::vector<DrawCommand> draw_commands;
};

class Scene final {
public:
  Scene(std::size_t object_count, std::uint64_t seed);

  [[nodiscard]] auto process_view(const Frustum& frustum, std::uint64_t query_seed) const
      -> SceneQueryResult;
  [[nodiscard]] auto brute_force_visible_count(const Frustum& frustum) const -> std::size_t;
  [[nodiscard]] auto query_aabb(const Aabb& bounds) const -> std::vector<std::uint32_t>;
  [[nodiscard]] auto object_count() const -> std::size_t;
  [[nodiscard]] auto node_count() const -> std::size_t;

private:
  struct SceneObject {
    Aabb bounds;
    std::uint32_t mesh_index{};
  };

  struct BvhNode {
    Aabb bounds;
    std::uint32_t begin{};
    std::uint32_t count{};
    std::uint32_t left{};
    std::uint32_t right{};
    bool leaf{};
  };

  void build_bvh();
  [[nodiscard]] auto build_node(std::uint32_t begin, std::uint32_t end) -> std::uint32_t;
  void query_aabb_node(std::uint32_t node_index, const Aabb& bounds,
                       std::vector<std::uint32_t>& output) const;
  void query_frustum_node(std::uint32_t node_index, const Frustum& frustum,
                          SceneQueryResult& result) const;

  std::vector<SceneObject> objects_;
  std::vector<std::uint32_t> object_order_;
  std::vector<BvhNode> nodes_;
  std::uint32_t root_index_{};
};

} // namespace gamebench::simulation
