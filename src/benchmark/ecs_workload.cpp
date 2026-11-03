#include "benchmark/ecs_workload.hpp"

#include "runtime/job_system.hpp"
#include "runtime/parallel.hpp"
#include "simulation/ecs.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

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

EcsWorkload::EcsWorkload() = default;
EcsWorkload::~EcsWorkload() = default;

auto EcsWorkload::id() const -> std::string_view {
  return "ecs";
}

auto EcsWorkload::version() const -> std::string_view {
  return "1";
}

auto EcsWorkload::category() const -> std::string_view {
  return "synthetic";
}

auto EcsWorkload::description() const -> std::string_view {
  return "AoS、SoA 与 AoSoA 实体创建、更新、销毁和组件访问";
}

auto EcsWorkload::execute(const WorkloadConfig& config) -> WorkloadOutput {
  ensure_system(config);
  simulation::EcsWorld world(config.layout, static_cast<std::size_t>(config.objects));
  std::vector<simulation::EntityId> entities;
  entities.reserve(static_cast<std::size_t>(config.objects));
  for (std::uint64_t index = 0; index < config.objects; ++index) {
    entities.push_back(world.create_entity());
  }

  const auto chunk_size =
      std::max<std::size_t>(256, static_cast<std::size_t>(config.objects) /
                                     std::max<unsigned>(1U, config.thread_count * 8U));
  for (unsigned pass = 0; pass < 3; ++pass) {
    runtime::parallel_for(
        *system_, static_cast<std::size_t>(config.objects), chunk_size,
        [&world](std::size_t begin, std::size_t end) { world.update(begin, end, 1.0F / 60.0F); });
  }

  const auto checksum_jobs = std::max<unsigned>(1U, config.thread_count * 4U);
  const auto checksum_chunk =
      (static_cast<std::size_t>(config.objects) + checksum_jobs - 1U) / checksum_jobs;
  std::vector<std::uint64_t> partial_checksums(checksum_jobs, 0);
  runtime::parallel_for(
      *system_, static_cast<std::size_t>(config.objects), checksum_chunk,
      [&world, &partial_checksums, checksum_chunk](std::size_t begin, std::size_t end) {
        partial_checksums[begin / checksum_chunk] = world.checksum(begin, end);
      });

  const auto random_accesses = static_cast<std::size_t>(config.objects) * 2U;
  std::uint64_t checksum = mix64(config.seed);
  for (const auto partial : partial_checksums) {
    checksum = mix64(checksum ^ partial);
  }
  checksum = mix64(checksum ^ world.random_access_checksum(config.seed, random_accesses));

  world.destroy_every(7);
  const auto expected_alive =
      static_cast<std::size_t>(config.objects) - static_cast<std::size_t>(config.objects) / 7U;

  WorkloadOutput output;
  output.checksum = checksum;
  output.operations = config.objects * 4U + random_accesses;
  output.valid = world.alive_count() == expected_alive && world.alive(entities.front());
  if (!output.valid) {
    output.validation_message = "ECS 实体存活数量或组件数据校验失败";
  }
  set_job_execution_metadata(output, config);
  return output;
}

void EcsWorkload::ensure_system(const WorkloadConfig& config) {
  if (system_ && prepared_threads_ == config.thread_count &&
      prepared_scheduler_ == config.scheduler) {
    return;
  }
  system_.reset();
  prepared_threads_ = config.thread_count;
  prepared_scheduler_ = config.scheduler;
  system_ = std::make_unique<runtime::JobSystem>(runtime::JobSystemConfig{
      .worker_count = job_worker_count(config),
      .main_thread_participates = config.thread_count > 0,
      .scheduler = config.scheduler,
      .cpu_affinity = worker_affinity(config),
  });
}

} // namespace gamebench::benchmark
