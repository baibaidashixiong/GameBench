#include "simulation/simd_particles.hpp"

#include <gtest/gtest.h>

#include <string_view>

TEST(SimdParticleSystemTest, ReportsCompiledBackend) {
  const std::string_view backend = gamebench::simulation::simd_backend_name();
  EXPECT_FALSE(backend.empty());
}

TEST(SimdParticleSystemTest, SameSeedProducesDeterministicChecksum) {
  gamebench::simulation::SimdParticleSystem first(1024, 0x5eed);
  gamebench::simulation::SimdParticleSystem second(1024, 0x5eed);

  first.update(0, first.block_count(), 4, 1.0F / 120.0F);
  second.update(0, second.block_count(), 4, 1.0F / 120.0F);

  EXPECT_EQ(first.checksum(0, first.block_count()), second.checksum(0, second.block_count()));
}

TEST(SimdParticleSystemTest, SeedAndTimeStepAffectChecksum) {
  gamebench::simulation::SimdParticleSystem baseline(1024, 0x5eed);
  gamebench::simulation::SimdParticleSystem other_seed(1024, 0x6eed);
  gamebench::simulation::SimdParticleSystem other_time_step(1024, 0x5eed);

  baseline.update(0, baseline.block_count(), 4, 1.0F / 120.0F);
  other_seed.update(0, other_seed.block_count(), 4, 1.0F / 120.0F);
  other_time_step.update(0, other_time_step.block_count(), 4, 1.0F / 60.0F);

  EXPECT_NE(baseline.checksum(0, baseline.block_count()),
            other_seed.checksum(0, other_seed.block_count()));
  EXPECT_NE(baseline.checksum(0, baseline.block_count()),
            other_time_step.checksum(0, other_time_step.block_count()));
}

TEST(SimdParticleSystemTest, UpdateChangesObservableState) {
  gamebench::simulation::SimdParticleSystem particles(1024, 0x5eed);
  const auto initial = particles.checksum(0, particles.block_count());

  particles.update(0, particles.block_count(), 4, 1.0F / 120.0F);

  EXPECT_NE(initial, particles.checksum(0, particles.block_count()));
}
