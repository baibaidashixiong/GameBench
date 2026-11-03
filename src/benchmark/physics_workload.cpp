#include "benchmark/physics_workload.hpp"

#include "runtime/job_system.hpp"
#include "runtime/parallel.hpp"
#include "simulation/physics.hpp"

#include <algorithm>
#include <cstdint>

namespace gamebench::benchmark {
namespace {

[[nodiscard]] constexpr auto mix64(std::uint64_t value) -> std::uint64_t {
  value ^= value >> 30U;
  value *= 0xbf58476d1ce4e5b9ULL;
  value ^= value >> 27U;
  value *= 0x94d049bb133111ebULL;
  value ^= value >> 31U;
  return value;
}

} // namespace

PhysicsWorkload::PhysicsWorkload() = default;
PhysicsWorkload::~PhysicsWorkload() = default;

auto PhysicsWorkload::id() const -> std::string_view {
  return "physics";
}
auto PhysicsWorkload::version() const -> std::string_view {
  return "1";
}
auto PhysicsWorkload::category() const -> std::string_view {
  return "synthetic";
}
auto PhysicsWorkload::description() const -> std::string_view {
  return "刚体积分、Sweep-and-Prune Broad-phase、碰撞检测与空间查询";
}

auto PhysicsWorkload::execute(const WorkloadConfig& config) -> WorkloadOutput {
  ensure_system(config);
  simulation::PhysicsWorld world(static_cast<std::size_t>(config.objects), config.seed);
  const auto grain = std::max<std::size_t>(128, static_cast<std::size_t>(config.objects) /
                                                    std::max(1U, config.thread_count * 8U));
  for (unsigned step = 0; step < 4; ++step) {
    runtime::parallel_for(*system_, world.body_count(), grain,
                          [&world](std::size_t begin, std::size_t end) {
                            world.integrate(begin, end, 1.0F / 120.0F);
                          });
  }
  const auto collisions = world.collect_collisions();
  const auto spatial_hits = world.spatial_query({20.0F, 20.0F, 1.0F}, 12.0F);
  const auto body_checksum = world.checksum(0, world.body_count());

  WorkloadOutput output;
  output.checksum = mix64(body_checksum ^ collisions.broad_phase_pairs ^
                          (collisions.contact_pairs << 16U) ^ (spatial_hits << 32U));
  output.operations = world.body_count() * 4U + collisions.broad_phase_pairs + spatial_hits;
  output.valid = collisions.contact_pairs <= collisions.broad_phase_pairs && spatial_hits > 0;
  if (!output.valid) {
    output.validation_message = "物理碰撞或空间查询校验失败";
  }
  return output;
}

void PhysicsWorkload::ensure_system(const WorkloadConfig& config) {
  if (system_ && prepared_threads_ == config.thread_count &&
      prepared_scheduler_ == config.scheduler) {
    return;
  }
  system_.reset();
  prepared_threads_ = config.thread_count;
  prepared_scheduler_ = config.scheduler;
  system_ = std::make_unique<runtime::JobSystem>(runtime::JobSystemConfig{
      .worker_count = std::max(1U, config.thread_count - 1U),
      .main_thread_participates = config.thread_count > 1,
      .scheduler = config.scheduler,
      .cpu_affinity = {},
  });
}

} // namespace gamebench::benchmark
