#include "simulation/ecs.hpp"

#include <bit>
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

[[nodiscard]] auto float_bits(float value) -> std::uint32_t {
  return std::bit_cast<std::uint32_t>(value);
}

void combine_components(std::uint64_t& checksum, const Transform& transform,
                        const Velocity& velocity, const EntityState& state) {
  for (const auto value : transform.position) {
    checksum = mix64(checksum ^ float_bits(value));
  }
  for (const auto value : transform.scale) {
    checksum = mix64(checksum ^ float_bits(value));
  }
  for (const auto value : velocity.linear) {
    checksum = mix64(checksum ^ float_bits(value));
  }
  checksum = mix64(checksum ^ state.flags);
}

} // namespace

EcsWorld::EcsWorld(core::DataLayout layout, std::size_t capacity)
    : layout_(layout), capacity_(capacity) {
  if (capacity_ == 0) {
    throw std::invalid_argument("ECS 容量必须大于零");
  }
  slots_.reserve(capacity_);
  free_indices_.reserve(capacity_);
  switch (layout_) {
  case core::DataLayout::AoS:
    aos_.resize(capacity_);
    break;
  case core::DataLayout::SoA:
    soa_.transforms.resize(capacity_);
    soa_.velocities.resize(capacity_);
    soa_.states.resize(capacity_);
    soa_.alive.resize(capacity_);
    break;
  case core::DataLayout::AoSoA: {
    const auto block_count = (capacity_ + kAoSoABlockSize - 1) / kAoSoABlockSize;
    aosoa_.transforms.resize(block_count);
    aosoa_.velocities.resize(block_count);
    aosoa_.states.resize(block_count);
    aosoa_.alive.resize(block_count);
    break;
  }
  }
}

auto EcsWorld::create_entity() -> EntityId {
  EntityIndex index = 0;
  if (!free_indices_.empty()) {
    index = free_indices_.back();
    free_indices_.pop_back();
    ++slots_[index].generation;
  } else {
    if (slots_.size() >= capacity_) {
      throw std::overflow_error("ECS 容量已满");
    }
    index = static_cast<EntityIndex>(slots_.size());
    slots_.push_back(EntitySlot{});
  }
  slots_[index].alive = true;
  initialize_components(index);
  ++alive_count_;
  return (static_cast<EntityId>(slots_[index].generation) << 32U) | index;
}

void EcsWorld::destroy_entity(EntityId entity) {
  const auto index = static_cast<EntityIndex>(entity & 0xffffffffULL);
  const auto generation = static_cast<std::uint32_t>(entity >> 32U);
  if (index >= slots_.size() || !slots_[index].alive || slots_[index].generation != generation) {
    return;
  }
  slots_[index].alive = false;
  switch (layout_) {
  case core::DataLayout::AoS:
    aos_[index].alive = false;
    break;
  case core::DataLayout::SoA:
    soa_.alive[index] = false;
    break;
  case core::DataLayout::AoSoA:
    aosoa_.alive[index / kAoSoABlockSize][index % kAoSoABlockSize] = false;
    break;
  }
  free_indices_.push_back(index);
  --alive_count_;
}

auto EcsWorld::alive(EntityId entity) const -> bool {
  const auto index = static_cast<EntityIndex>(entity & 0xffffffffULL);
  const auto generation = static_cast<std::uint32_t>(entity >> 32U);
  return index < slots_.size() && slots_[index].alive && slots_[index].generation == generation;
}

auto EcsWorld::alive_count() const -> std::size_t {
  return alive_count_;
}

auto EcsWorld::capacity() const -> std::size_t {
  return capacity_;
}

void EcsWorld::update(std::size_t begin, std::size_t end, float delta_time) {
  end = std::min(end, slots_.size());
  for (auto index = begin; index < end; ++index) {
    if (!slots_[index].alive) {
      continue;
    }
    const auto entity_index = static_cast<EntityIndex>(index);
    switch (layout_) {
    case core::DataLayout::AoS:
      update_entity_aos(entity_index, delta_time);
      break;
    case core::DataLayout::SoA:
      update_entity_soa(entity_index, delta_time);
      break;
    case core::DataLayout::AoSoA:
      update_entity_aosoa(entity_index, delta_time);
      break;
    }
  }
}

void EcsWorld::destroy_every(std::size_t stride) {
  if (stride == 0) {
    throw std::invalid_argument("销毁步长必须大于零");
  }
  std::size_t ordinal = 0;
  for (std::size_t index = 0; index < slots_.size(); ++index) {
    if (!slots_[index].alive) {
      continue;
    }
    if (++ordinal % stride == 0) {
      destroy_entity((static_cast<EntityId>(slots_[index].generation) << 32U) | index);
    }
  }
}

auto EcsWorld::checksum(std::size_t begin, std::size_t end) const -> std::uint64_t {
  end = std::min(end, slots_.size());
  std::uint64_t result = 0x9e3779b97f4a7c15ULL;
  for (auto index = begin; index < end; ++index) {
    if (!slots_[index].alive) {
      continue;
    }
    const auto entity_index = static_cast<EntityIndex>(index);
    switch (layout_) {
    case core::DataLayout::AoS:
      result = mix64(result ^ checksum_entity_aos(entity_index));
      break;
    case core::DataLayout::SoA:
      result = mix64(result ^ checksum_entity_soa(entity_index));
      break;
    case core::DataLayout::AoSoA:
      result = mix64(result ^ checksum_entity_aosoa(entity_index));
      break;
    }
  }
  return result;
}

auto EcsWorld::random_access_checksum(std::uint64_t seed, std::size_t accesses) const
    -> std::uint64_t {
  std::uint64_t state = seed;
  std::uint64_t result = 0;
  for (std::size_t access = 0; access < accesses; ++access) {
    state = mix64(state + access);
    const auto index = static_cast<EntityIndex>(state % slots_.size());
    switch (layout_) {
    case core::DataLayout::AoS:
      result = mix64(result ^ random_record_aos(index));
      break;
    case core::DataLayout::SoA:
      result = mix64(result ^ random_record_soa(index));
      break;
    case core::DataLayout::AoSoA:
      result = mix64(result ^ random_record_aosoa(index));
      break;
    }
  }
  return result;
}

void EcsWorld::initialize_components(EntityIndex index) {
  const auto value = static_cast<float>(index);
  Transform transform;
  transform.position[0] = value * 0.001F;
  transform.position[1] = value * 0.002F;
  transform.position[2] = value * 0.003F;
  Velocity velocity;
  velocity.linear[0] = 0.25F + value * 0.0001F;
  velocity.linear[1] = 0.5F;
  velocity.linear[2] = -0.125F;
  EntityState state;
  state.flags = static_cast<std::uint32_t>(index % 17U);

  switch (layout_) {
  case core::DataLayout::AoS:
    aos_[index].transform = transform;
    aos_[index].velocity = velocity;
    aos_[index].state = state;
    aos_[index].alive = true;
    break;
  case core::DataLayout::SoA:
    soa_.transforms[index] = transform;
    soa_.velocities[index] = velocity;
    soa_.states[index] = state;
    soa_.alive[index] = true;
    break;
  case core::DataLayout::AoSoA: {
    const auto block = index / kAoSoABlockSize;
    const auto offset = index % kAoSoABlockSize;
    aosoa_.transforms[block][offset] = transform;
    aosoa_.velocities[block][offset] = velocity;
    aosoa_.states[block][offset] = state;
    aosoa_.alive[block][offset] = true;
    break;
  }
  }
}

void EcsWorld::update_entity_aos(EntityIndex index, float delta_time) {
  auto& record = aos_[index];
  for (std::size_t axis = 0; axis < 3; ++axis) {
    record.transform.position[axis] += record.velocity.linear[axis] * delta_time;
  }
  record.state.flags = static_cast<std::uint32_t>((record.state.flags + 1U) % 251U);
}

void EcsWorld::update_entity_soa(EntityIndex index, float delta_time) {
  for (std::size_t axis = 0; axis < 3; ++axis) {
    soa_.transforms[index].position[axis] += soa_.velocities[index].linear[axis] * delta_time;
  }
  soa_.states[index].flags = static_cast<std::uint32_t>((soa_.states[index].flags + 1U) % 251U);
}

void EcsWorld::update_entity_aosoa(EntityIndex index, float delta_time) {
  const auto block = index / kAoSoABlockSize;
  const auto offset = index % kAoSoABlockSize;
  for (std::size_t axis = 0; axis < 3; ++axis) {
    aosoa_.transforms[block][offset].position[axis] +=
        aosoa_.velocities[block][offset].linear[axis] * delta_time;
  }
  aosoa_.states[block][offset].flags =
      static_cast<std::uint32_t>((aosoa_.states[block][offset].flags + 1U) % 251U);
}

auto EcsWorld::checksum_entity_aos(EntityIndex index) const -> std::uint64_t {
  const auto& record = aos_[index];
  std::uint64_t result = 0;
  combine_components(result, record.transform, record.velocity, record.state);
  return result;
}

auto EcsWorld::checksum_entity_soa(EntityIndex index) const -> std::uint64_t {
  std::uint64_t result = 0;
  combine_components(result, soa_.transforms[index], soa_.velocities[index], soa_.states[index]);
  return result;
}

auto EcsWorld::checksum_entity_aosoa(EntityIndex index) const -> std::uint64_t {
  const auto block = index / kAoSoABlockSize;
  const auto offset = index % kAoSoABlockSize;
  std::uint64_t result = 0;
  combine_components(result, aosoa_.transforms[block][offset], aosoa_.velocities[block][offset],
                     aosoa_.states[block][offset]);
  return result;
}

auto EcsWorld::random_record_aos(EntityIndex index) const -> std::uint64_t {
  return checksum_entity_aos(index);
}

auto EcsWorld::random_record_soa(EntityIndex index) const -> std::uint64_t {
  return checksum_entity_soa(index);
}

auto EcsWorld::random_record_aosoa(EntityIndex index) const -> std::uint64_t {
  return checksum_entity_aosoa(index);
}

} // namespace gamebench::simulation
