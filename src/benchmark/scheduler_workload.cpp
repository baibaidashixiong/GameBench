#include "benchmark/scheduler_workload.hpp"

#include "runtime/job_system.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <numeric>
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

[[nodiscard]] auto busy_sum(const runtime::SchedulerStatistics& statistics) -> double {
  return std::accumulate(statistics.worker_busy_ms.begin(), statistics.worker_busy_ms.end(), 0.0);
}

} // namespace

SchedulerWorkload::SchedulerWorkload() = default;
SchedulerWorkload::~SchedulerWorkload() = default;

auto SchedulerWorkload::id() const -> std::string_view {
  return "scheduler";
}

auto SchedulerWorkload::version() const -> std::string_view {
  return "1";
}

auto SchedulerWorkload::category() const -> std::string_view {
  return "synthetic";
}

auto SchedulerWorkload::description() const -> std::string_view {
  return "相同任务图下的全局队列、静态分区与 Work Stealing 调度负载";
}

auto SchedulerWorkload::execute(const WorkloadConfig& config) -> WorkloadOutput {
  ensure_system(config);

  constexpr std::uint64_t shard_count = 64;
  std::vector<std::uint64_t> shard_results(shard_count, 0);
  std::vector<runtime::JobHandle> shards;
  shards.reserve(shard_count);

  const auto base = config.objects / shard_count;
  const auto remainder = config.objects % shard_count;
  for (std::uint64_t shard = 0; shard < shard_count; ++shard) {
    const auto count = base + (shard < remainder ? 1ULL : 0ULL);
    shards.push_back(system_->submit([&, shard, count] {
      std::uint64_t accumulator = config.seed ^ (shard * 0x9e3779b97f4a7c15ULL);
      for (std::uint64_t index = 0; index < count; ++index) {
        accumulator += mix64(config.seed + shard * count + index);
        accumulator ^= accumulator >> 19U;
      }
      shard_results[shard] = accumulator;
    }));
  }

  std::uint64_t checksum = 0;
  std::uint64_t completed_shards = 0;
  auto merge = system_->submit(
      [&] {
        checksum = mix64(config.seed);
        for (const auto value : shard_results) {
          checksum = mix64(checksum ^ value);
          ++completed_shards;
        }
      },
      shards);
  system_->wait(merge);

  const auto current = system_->statistics();
  const auto previous_busy = has_previous_statistics_ ? previous_cumulative_.worker_busy_ms : 0.0;
  WorkloadOutput output;
  output.checksum = checksum;
  output.operations = config.objects;
  output.valid = completed_shards == shard_count;
  if (!output.valid) {
    output.validation_message = "调度工作负载分片数量不匹配";
  }
  set_job_execution_metadata(output, config);
  output.scheduling = SchedulingMetrics{
      .submitted_jobs = current.submitted_jobs -
                        (has_previous_statistics_ ? previous_cumulative_.submitted_jobs : 0),
      .completed_jobs = current.completed_jobs -
                        (has_previous_statistics_ ? previous_cumulative_.completed_jobs : 0),
      .stolen_jobs =
          current.stolen_jobs - (has_previous_statistics_ ? previous_cumulative_.stolen_jobs : 0),
      .main_thread_jobs = current.main_thread_jobs -
                          (has_previous_statistics_ ? previous_cumulative_.main_thread_jobs : 0),
      .exceptions =
          current.exceptions - (has_previous_statistics_ ? previous_cumulative_.exceptions : 0),
      .worker_busy_ms = busy_sum(current) - previous_busy,
  };
  previous_cumulative_ = *output.scheduling;
  has_previous_statistics_ = true;
  return output;
}

void SchedulerWorkload::ensure_system(const WorkloadConfig& config) {
  if (system_ && prepared_threads_ == config.thread_count &&
      prepared_scheduler_ == config.scheduler) {
    return;
  }

  system_.reset();
  prepared_threads_ = config.thread_count;
  prepared_scheduler_ = config.scheduler;
  previous_cumulative_ = {};
  has_previous_statistics_ = false;
  const auto worker_count = job_worker_count(config);
  system_ = std::make_unique<runtime::JobSystem>(runtime::JobSystemConfig{
      .worker_count = worker_count,
      .main_thread_participates = config.thread_count > 0,
      .scheduler = config.scheduler,
      .cpu_affinity = worker_affinity(config),
  });
}

} // namespace gamebench::benchmark
