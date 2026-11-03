#include "benchmark/memory_workload.hpp"

#include "runtime/job_system.hpp"
#include "runtime/parallel.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <numeric>
#include <stdexcept>
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

MemoryWorkload::MemoryWorkload() = default;
MemoryWorkload::~MemoryWorkload() = default;

auto MemoryWorkload::id() const -> std::string_view {
  return "memory";
}

auto MemoryWorkload::version() const -> std::string_view {
  return "1";
}

auto MemoryWorkload::category() const -> std::string_view {
  return "synthetic";
}

auto MemoryWorkload::description() const -> std::string_view {
  return "顺序、随机、跨步与 Pointer Chasing 内存访问";
}

auto MemoryWorkload::execute(const WorkloadConfig& config) -> WorkloadOutput {
  ensure_system(config);
  ensure_storage(config);

  const auto shard_count = std::max<unsigned>(1U, config.thread_count * 4U);
  std::vector<std::uint64_t> partials(shard_count, 0);
  const auto element_count = payload_.size();
  const auto random_accesses = element_count * 4U;

  runtime::parallel_for(
      *system_, shard_count, 1, [&, random_accesses](std::size_t begin, std::size_t end) {
        for (auto shard = begin; shard < end; ++shard) {
          std::uint64_t checksum = mix64(config.seed + shard);
          if (config.memory_pattern == core::MemoryPattern::Sequential) {
            const auto range_begin = element_count * shard / shard_count;
            const auto range_end = element_count * (shard + 1U) / shard_count;
            for (auto index = range_begin; index < range_end; ++index) {
              checksum = mix64(checksum + payload_[index]);
            }
          } else if (config.memory_pattern == core::MemoryPattern::Strided) {
            auto index = shard % 8U;
            while (index < element_count) {
              checksum = mix64(checksum ^ payload_[index]);
              index += 8U;
            }
          } else if (config.memory_pattern == core::MemoryPattern::Random) {
            auto state = config.seed ^ (shard * 0x9e3779b97f4a7c15ULL);
            const auto count = random_accesses / shard_count + 1U;
            for (std::size_t access = 0; access < count; ++access) {
              state = mix64(state + access);
              checksum += payload_[state % element_count];
            }
          } else {
            auto index = static_cast<std::size_t>(mix64(config.seed + shard) % element_count);
            const auto count = random_accesses / shard_count + 1U;
            for (std::size_t access = 0; access < count; ++access) {
              index = links_[index];
              checksum ^= payload_[index];
              checksum = (checksum << 7U) | (checksum >> 57U);
            }
          }
          partials[shard] = checksum;
        }
      });

  std::uint64_t checksum = mix64(config.seed ^ static_cast<std::uint64_t>(config.memory_pattern));
  for (const auto partial : partials) {
    checksum = mix64(checksum ^ partial);
  }

  WorkloadOutput output;
  output.checksum = checksum;
  output.operations = config.memory_pattern == core::MemoryPattern::Sequential ||
                              config.memory_pattern == core::MemoryPattern::Strided
                          ? element_count
                          : random_accesses + shard_count;
  output.valid = output.operations > 0 && !payload_.empty();
  if (!output.valid) {
    output.validation_message = "内存工作负载未执行有效访问";
  }
  set_job_execution_metadata(output, config);
  return output;
}

void MemoryWorkload::ensure_system(const WorkloadConfig& config) {
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

void MemoryWorkload::ensure_storage(const WorkloadConfig& config) {
  if (!payload_.empty() && prepared_bytes_ == config.working_set_bytes &&
      prepared_seed_ == config.seed) {
    return;
  }
  if (config.working_set_bytes < sizeof(std::uint64_t)) {
    throw std::invalid_argument("working_set_bytes 至少为 8 字节");
  }
  const auto element_count =
      static_cast<std::size_t>(config.working_set_bytes / sizeof(std::uint64_t));
  if (element_count > std::numeric_limits<std::uint32_t>::max()) {
    throw std::invalid_argument("当前 Pointer Chasing 实现最多支持 32 位元素数量");
  }
  payload_.resize(element_count);
  for (std::size_t index = 0; index < payload_.size(); ++index) {
    payload_[index] = mix64(config.seed + index);
  }

  links_.resize(element_count);
  std::vector<std::uint32_t> order(element_count);
  std::iota(order.begin(), order.end(), 0U);
  auto state = mix64(config.seed ^ 0xa5a5a5a5a5a5a5a5ULL);
  for (std::size_t index = order.size(); index > 1; --index) {
    state = mix64(state + index);
    const auto selected = static_cast<std::size_t>(state % index);
    std::swap(order[index - 1], order[selected]);
  }
  for (std::size_t index = 0; index < order.size(); ++index) {
    links_[order[index]] = order[(index + 1U) % order.size()];
  }
  prepared_bytes_ = config.working_set_bytes;
  prepared_seed_ = config.seed;
}

} // namespace gamebench::benchmark
