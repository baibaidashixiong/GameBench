#include "benchmark/ai_workload.hpp"

#include "runtime/job_system.hpp"
#include "runtime/parallel.hpp"
#include "simulation/navigation.hpp"

#include <algorithm>
#include <cmath>
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

AiWorkload::AiWorkload() = default;
AiWorkload::~AiWorkload() = default;
auto AiWorkload::id() const -> std::string_view {
  return "ai";
}
auto AiWorkload::version() const -> std::string_view {
  return "1";
}
auto AiWorkload::category() const -> std::string_view {
  return "synthetic";
}
auto AiWorkload::description() const -> std::string_view {
  return "Agent 行为决策、A* Navigation 查询与路径生成";
}

auto AiWorkload::execute(const WorkloadConfig& config) -> WorkloadOutput {
  ensure_system(config);
  constexpr std::uint32_t grid_size = 64;
  simulation::NavigationGrid grid(grid_size, grid_size, config.seed);
  std::vector<std::uint64_t> partials(static_cast<std::size_t>(config.objects), 0);
  std::vector<std::uint64_t> path_lengths(static_cast<std::size_t>(config.objects), 0);
  const auto grain = std::max<std::size_t>(32, static_cast<std::size_t>(config.objects) /
                                                   std::max(1U, config.thread_count * 8U));

  runtime::parallel_for(*system_, partials.size(), grain, [&](std::size_t begin, std::size_t end) {
    for (auto agent = begin; agent < end; ++agent) {
      const auto state = mix64(config.seed + agent);
      const auto goal_x = static_cast<std::uint16_t>(1U + state % (grid_size - 1U));
      const auto goal_y = static_cast<std::uint16_t>(1U + (state >> 16U) % (grid_size - 1U));
      const auto path = grid.find_path({0, 0}, {goal_x, goal_y});
      path_lengths[agent] = path.size();
      const auto behavior = simulation::evaluate_behavior(
          {.position_x = static_cast<float>(agent % grid_size),
           .position_y = static_cast<float>((agent / grid_size) % grid_size),
           .health = static_cast<float>(state & 0xffU),
           .distance_to_target = static_cast<float>(path.size())});
      auto checksum = mix64(state ^ path.size());
      checksum = mix64(checksum ^ static_cast<std::uint8_t>(behavior));
      partials[agent] = checksum;
    }
  });

  std::uint64_t checksum = mix64(config.seed);
  std::uint64_t total_path_cells = 0;
  for (const auto partial : partials) {
    checksum = mix64(checksum ^ partial);
  }
  for (const auto path_length : path_lengths) {
    total_path_cells += path_length;
  }
  WorkloadOutput output;
  output.checksum = checksum;
  output.operations = config.objects + total_path_cells;
  output.valid = config.objects > 0 && checksum != 0;
  if (!output.valid) {
    output.validation_message = "AI 行为或导航查询校验失败";
  }
  set_job_execution_metadata(output, config);
  return output;
}

void AiWorkload::ensure_system(const WorkloadConfig& config) {
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
