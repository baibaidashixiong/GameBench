#include "benchmark/animation_workload.hpp"

#include "runtime/job_system.hpp"
#include "runtime/parallel.hpp"
#include "simulation/animation.hpp"

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

AnimationWorkload::AnimationWorkload() = default;
AnimationWorkload::~AnimationWorkload() = default;
auto AnimationWorkload::id() const -> std::string_view {
  return "animation";
}
auto AnimationWorkload::version() const -> std::string_view {
  return "1";
}
auto AnimationWorkload::category() const -> std::string_view {
  return "synthetic";
}
auto AnimationWorkload::description() const -> std::string_view {
  return "动画采样、混合、骨骼层级与 Transform 传播";
}

auto AnimationWorkload::execute(const WorkloadConfig& config) -> WorkloadOutput {
  ensure_system(config);
  constexpr std::size_t bone_count = 32;
  std::vector<std::uint64_t> partials(static_cast<std::size_t>(config.objects), 0);
  const auto grain = std::max<std::size_t>(64, static_cast<std::size_t>(config.objects) /
                                                   std::max(1U, config.thread_count * 8U));
  runtime::parallel_for(*system_, partials.size(), grain, [&](std::size_t begin, std::size_t end) {
    for (auto skeleton = begin; skeleton < end; ++skeleton) {
      partials[skeleton] = simulation::evaluate_skeleton(
          config.seed + skeleton, bone_count,
          static_cast<float>(config.iterations + skeleton % 31U) / 60.0F);
    }
  });

  std::uint64_t checksum = mix64(config.seed);
  for (const auto partial : partials) {
    checksum = mix64(checksum ^ partial);
  }
  WorkloadOutput output;
  output.checksum = checksum;
  output.operations = config.objects * bone_count;
  output.valid = config.objects > 0 && checksum != 0;
  if (!output.valid) {
    output.validation_message = "动画骨骼传播校验失败";
  }
  return output;
}

void AnimationWorkload::ensure_system(const WorkloadConfig& config) {
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
