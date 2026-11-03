#include "benchmark/scene_workload.hpp"

#include "runtime/job_system.hpp"
#include "runtime/parallel.hpp"
#include "simulation/scene.hpp"

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

SceneWorkload::SceneWorkload() = default;
SceneWorkload::~SceneWorkload() = default;

auto SceneWorkload::id() const -> std::string_view {
  return "scene";
}

auto SceneWorkload::version() const -> std::string_view {
  return "1";
}

auto SceneWorkload::category() const -> std::string_view {
  return "synthetic";
}

auto SceneWorkload::description() const -> std::string_view {
  return "BVH、AABB 查询、Frustum Culling、LOD 与 Draw Call 准备";
}

auto SceneWorkload::execute(const WorkloadConfig& config) -> WorkloadOutput {
  ensure_system(config);
  ensure_scene(config);

  constexpr unsigned shard_count = 64;
  struct ShardResult {
    std::uint64_t visible{};
    std::uint64_t spatial_hits{};
    std::uint64_t checksum{};
  };
  std::vector<ShardResult> shards(shard_count);

  runtime::parallel_for(*system_, shard_count, 1, [&](std::size_t begin, std::size_t end) {
    for (auto shard = begin; shard < end; ++shard) {
      auto frustum = simulation::Frustum::perspective(1.0F, 100.0F, 0.7002075F);
      const auto x_offset = static_cast<float>(static_cast<int>(shard % 8U) - 4) * 4.0F;
      const auto y_offset = static_cast<float>(static_cast<int>(shard / 8U) - 2) * 4.0F;
      frustum = frustum.translated({x_offset, y_offset, 0.0F});
      const auto query = scene_->process_view(frustum, config.seed + shard);
      shards[shard] = {.visible = query.visible_objects,
                       .spatial_hits = query.spatial_hits,
                       .checksum = query.checksum};
    }
  });

  std::uint64_t checksum = mix64(config.seed);
  std::uint64_t visible = 0;
  std::uint64_t spatial_hits = 0;
  for (const auto& shard : shards) {
    checksum = mix64(checksum ^ shard.checksum);
    visible += shard.visible;
    spatial_hits += shard.spatial_hits;
  }

  WorkloadOutput output;
  output.checksum = checksum;
  output.operations = config.objects * shard_count;
  output.valid = scene_valid_ && visible > 0 && spatial_hits > 0;
  if (!output.valid) {
    output.validation_message = "场景 BVH 与暴力遍历结果不一致或查询为空";
  }
  set_job_execution_metadata(output, config);
  return output;
}

void SceneWorkload::ensure_system(const WorkloadConfig& config) {
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

void SceneWorkload::ensure_scene(const WorkloadConfig& config) {
  if (scene_ && prepared_objects_ == config.objects && prepared_seed_ == config.seed) {
    return;
  }
  prepared_objects_ = static_cast<std::size_t>(config.objects);
  prepared_seed_ = config.seed;
  scene_ = std::make_unique<simulation::Scene>(prepared_objects_, prepared_seed_);
  const auto frustum = simulation::Frustum::perspective(1.0F, 100.0F, 0.7002075F);
  const auto query = scene_->process_view(frustum, config.seed);
  scene_valid_ = query.visible_objects == scene_->brute_force_visible_count(frustum) &&
                 query.visible_objects == query.draw_commands.size() && query.spatial_hits > 0;
}

} // namespace gamebench::benchmark
