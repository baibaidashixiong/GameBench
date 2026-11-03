#pragma once

#include "core/config.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace gamebench::simulation {

using EntityId = std::uint64_t;
using EntityIndex = std::uint32_t;

struct Transform {
  float position[3]{};
  float scale[3]{1.0F, 1.0F, 1.0F};
};

struct Velocity {
  float linear[3]{};
};

struct EntityState {
  std::uint32_t flags{};
};

class EcsWorld final {
public:
  static constexpr std::size_t kAoSoABlockSize = 16;

  EcsWorld(core::DataLayout layout, std::size_t capacity);

  [[nodiscard]] auto create_entity() -> EntityId;
  void destroy_entity(EntityId entity);
  [[nodiscard]] auto alive(EntityId entity) const -> bool;
  [[nodiscard]] auto alive_count() const -> std::size_t;
  [[nodiscard]] auto capacity() const -> std::size_t;

  void update(std::size_t begin, std::size_t end, float delta_time);
  void destroy_every(std::size_t stride);
  [[nodiscard]] auto checksum(std::size_t begin, std::size_t end) const -> std::uint64_t;
  [[nodiscard]] auto random_access_checksum(std::uint64_t seed, std::size_t accesses) const
      -> std::uint64_t;

private:
  struct EntitySlot {
    std::uint32_t generation{};
    bool alive{};
  };

  struct AoSRecord {
    Transform transform;
    Velocity velocity;
    EntityState state;
    bool alive{};
  };

  struct SoAStorage {
    std::vector<Transform> transforms;
    std::vector<Velocity> velocities;
    std::vector<EntityState> states;
    std::vector<bool> alive;
  };

  struct AoSoAStorage {
    std::vector<std::array<Transform, kAoSoABlockSize>> transforms;
    std::vector<std::array<Velocity, kAoSoABlockSize>> velocities;
    std::vector<std::array<EntityState, kAoSoABlockSize>> states;
    std::vector<std::array<bool, kAoSoABlockSize>> alive;
  };

  void initialize_components(EntityIndex index);
  void update_entity_aos(EntityIndex index, float delta_time);
  void update_entity_soa(EntityIndex index, float delta_time);
  void update_entity_aosoa(EntityIndex index, float delta_time);
  [[nodiscard]] auto checksum_entity_aos(EntityIndex index) const -> std::uint64_t;
  [[nodiscard]] auto checksum_entity_soa(EntityIndex index) const -> std::uint64_t;
  [[nodiscard]] auto checksum_entity_aosoa(EntityIndex index) const -> std::uint64_t;
  [[nodiscard]] auto random_record_aos(EntityIndex index) const -> std::uint64_t;
  [[nodiscard]] auto random_record_soa(EntityIndex index) const -> std::uint64_t;
  [[nodiscard]] auto random_record_aosoa(EntityIndex index) const -> std::uint64_t;

  core::DataLayout layout_;
  std::size_t capacity_;
  std::vector<EntitySlot> slots_;
  std::vector<EntityIndex> free_indices_;
  std::size_t alive_count_{};
  std::vector<AoSRecord> aos_;
  SoAStorage soa_;
  AoSoAStorage aosoa_;
};

} // namespace gamebench::simulation
