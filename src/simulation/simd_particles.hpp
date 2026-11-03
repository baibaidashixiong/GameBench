#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace gamebench::simulation {

[[nodiscard]] auto simd_backend_name() -> const char*;

class SimdParticleSystem final {
public:
  SimdParticleSystem(std::size_t particle_count, std::uint64_t seed);

  void update(std::size_t begin_block, std::size_t end_block, unsigned steps, float delta_time);
  [[nodiscard]] auto checksum(std::size_t begin_block, std::size_t end_block) const
      -> std::uint64_t;
  [[nodiscard]] auto block_count() const -> std::size_t;

private:
  std::size_t particle_count_{};
  std::size_t block_count_{};
  std::vector<float> position_x_;
  std::vector<float> position_y_;
  std::vector<float> position_z_;
  std::vector<float> velocity_x_;
  std::vector<float> velocity_y_;
  std::vector<float> velocity_z_;
  std::vector<float> life_;
};

} // namespace gamebench::simulation
