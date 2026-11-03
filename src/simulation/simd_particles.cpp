#include "simulation/simd_particles.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <stdexcept>

#if defined(__SSE2__)
#include <emmintrin.h>
#endif

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

[[nodiscard]] auto random_float(std::uint64_t value, float minimum, float maximum) -> float {
  const auto unit = static_cast<float>(value & 0xffffU) / 65535.0F;
  return minimum + (maximum - minimum) * unit;
}

} // namespace

auto simd_backend_name() -> const char* {
#if defined(__SSE2__)
  return "sse2";
#else
  return "scalar-fallback";
#endif
}

SimdParticleSystem::SimdParticleSystem(std::size_t particle_count, std::uint64_t seed) {
  if (particle_count == 0) {
    throw std::invalid_argument("SIMD 粒子数量必须大于零");
  }
  particle_count_ = particle_count;
  block_count_ = (particle_count_ + 3U) / 4U;
  const auto storage_count = block_count_ * 4U;
  position_x_.resize(storage_count);
  position_y_.resize(storage_count);
  position_z_.resize(storage_count);
  velocity_x_.resize(storage_count);
  velocity_y_.resize(storage_count);
  velocity_z_.resize(storage_count);
  life_.resize(storage_count);
  for (std::size_t particle = 0; particle < storage_count; ++particle) {
    const auto state = mix64(seed + particle * 0x9e3779b97f4a7c15ULL);
    position_x_[particle] = random_float(state, -100.0F, 100.0F);
    position_y_[particle] = random_float(state >> 8U, -20.0F, 80.0F);
    position_z_[particle] = random_float(state >> 16U, 0.0F, 200.0F);
    velocity_x_[particle] = random_float(state >> 24U, -8.0F, 8.0F);
    velocity_y_[particle] = random_float(state >> 32U, -4.0F, 16.0F);
    velocity_z_[particle] = random_float(state >> 40U, 1.0F, 20.0F);
    life_[particle] = random_float(state >> 48U, 0.5F, 10.0F);
  }
}

void SimdParticleSystem::update(std::size_t begin_block, std::size_t end_block, unsigned steps,
                                float delta_time) {
  end_block = std::min(end_block, block_count_);
  begin_block = std::min(begin_block, end_block);
#if defined(__SSE2__)
  const auto dt = _mm_set1_ps(delta_time);
  const auto gravity = _mm_set1_ps(9.81F * delta_time);
  const auto damping = _mm_set1_ps(0.9995F);
  const auto zero = _mm_setzero_ps();
  const auto maximum_life = _mm_set1_ps(16.0F);
  const auto life_gain = _mm_set1_ps(0.0001F);
  for (std::size_t block = begin_block; block < end_block; ++block) {
    const auto index = block * 4U;
    auto position_x = _mm_loadu_ps(&position_x_[index]);
    auto position_y = _mm_loadu_ps(&position_y_[index]);
    auto position_z = _mm_loadu_ps(&position_z_[index]);
    auto velocity_x = _mm_loadu_ps(&velocity_x_[index]);
    auto velocity_y = _mm_loadu_ps(&velocity_y_[index]);
    auto velocity_z = _mm_loadu_ps(&velocity_z_[index]);
    auto life = _mm_loadu_ps(&life_[index]);
    for (unsigned step = 0; step < steps; ++step) {
      velocity_y = _mm_sub_ps(velocity_y, gravity);
      velocity_x = _mm_mul_ps(velocity_x, damping);
      velocity_y = _mm_mul_ps(velocity_y, damping);
      velocity_z = _mm_mul_ps(velocity_z, damping);
      position_x = _mm_add_ps(position_x, _mm_mul_ps(velocity_x, dt));
      position_y = _mm_add_ps(position_y, _mm_mul_ps(velocity_y, dt));
      position_z = _mm_add_ps(position_z, _mm_mul_ps(velocity_z, dt));
      life = _mm_max_ps(zero, _mm_sub_ps(life, dt));
      const auto speed = _mm_sqrt_ps(_mm_add_ps(
          _mm_add_ps(_mm_mul_ps(velocity_x, velocity_x), _mm_mul_ps(velocity_y, velocity_y)),
          _mm_mul_ps(velocity_z, velocity_z)));
      life = _mm_min_ps(maximum_life, _mm_add_ps(life, _mm_mul_ps(speed, life_gain)));
    }
    _mm_storeu_ps(&position_x_[index], position_x);
    _mm_storeu_ps(&position_y_[index], position_y);
    _mm_storeu_ps(&position_z_[index], position_z);
    _mm_storeu_ps(&velocity_x_[index], velocity_x);
    _mm_storeu_ps(&velocity_y_[index], velocity_y);
    _mm_storeu_ps(&velocity_z_[index], velocity_z);
    _mm_storeu_ps(&life_[index], life);
  }
#else
  for (std::size_t particle = begin_block * 4U;
       particle < std::min(end_block * 4U, particle_count_); ++particle) {
    for (unsigned step = 0; step < steps; ++step) {
      velocity_y_[particle] -= 9.81F * delta_time;
      velocity_x_[particle] *= 0.9995F;
      velocity_y_[particle] *= 0.9995F;
      velocity_z_[particle] *= 0.9995F;
      position_x_[particle] += velocity_x_[particle] * delta_time;
      position_y_[particle] += velocity_y_[particle] * delta_time;
      position_z_[particle] += velocity_z_[particle] * delta_time;
      life_[particle] = std::max(0.0F, life_[particle] - delta_time);
      const auto speed = std::sqrt(velocity_x_[particle] * velocity_x_[particle] +
                                   velocity_y_[particle] * velocity_y_[particle] +
                                   velocity_z_[particle] * velocity_z_[particle]);
      life_[particle] = std::min(16.0F, life_[particle] + speed * 0.0001F);
    }
  }
#endif
}

auto SimdParticleSystem::checksum(std::size_t begin_block, std::size_t end_block) const
    -> std::uint64_t {
  end_block = std::min(end_block, block_count_);
  begin_block = std::min(begin_block, end_block);
  std::uint64_t result = 0x9e3779b97f4a7c15ULL;
  for (std::size_t particle = begin_block * 4U;
       particle < std::min(end_block * 4U, particle_count_); ++particle) {
    result = mix64(result ^ std::bit_cast<std::uint32_t>(position_x_[particle]));
    result = mix64(result ^ std::bit_cast<std::uint32_t>(position_y_[particle]));
    result = mix64(result ^ std::bit_cast<std::uint32_t>(position_z_[particle]));
    result = mix64(result ^ std::bit_cast<std::uint32_t>(life_[particle]));
  }
  return result;
}

auto SimdParticleSystem::block_count() const -> std::size_t {
  return block_count_;
}

} // namespace gamebench::simulation
