#include "benchmark/vulkan_workload.hpp"

#include "core/timing.hpp"
#include "runtime/job_system.hpp"
#include "runtime/parallel.hpp"

#if defined(GAMEBENCH_HAS_VULKAN)
#include "renderer/vulkan_context.hpp"
#endif

#include <algorithm>
#include <cstdint>
#include <exception>
#include <string>
#include <vector>

namespace gamebench::benchmark {
namespace {

constexpr unsigned kCommandTasks = 64;

[[nodiscard]] constexpr auto mix64(std::uint64_t value) -> std::uint64_t {
  value ^= value >> 30U;
  value *= 0xbf58476d1ce4e5b9ULL;
  value ^= value >> 27U;
  value *= 0x94d049bb133111ebULL;
  value ^= value >> 31U;
  return value;
}

} // namespace

VulkanWorkload::VulkanWorkload() = default;
VulkanWorkload::~VulkanWorkload() = default;

auto VulkanWorkload::id() const -> std::string_view {
  return "vulkan";
}

auto VulkanWorkload::version() const -> std::string_view {
  return "1";
}

auto VulkanWorkload::category() const -> std::string_view {
  return "synthetic";
}

auto VulkanWorkload::description() const -> std::string_view {
  return "Vulkan Command Buffer 多线程录制与轻量 Offscreen 清屏";
}

auto VulkanWorkload::execute(const WorkloadConfig& config) -> WorkloadOutput {
  ensure_system(config);
#if !defined(GAMEBENCH_HAS_VULKAN)
  WorkloadOutput output;
  output.skipped = true;
  output.valid = false;
  output.validation_message = "Vulkan 不可用，工作负载已跳过: 构建时未找到 Vulkan SDK";
  return output;
#else
  if (!ensure_context(config)) {
    WorkloadOutput output;
    output.skipped = true;
    output.valid = false;
    output.validation_message = context_error_;
    return output;
  }

  const auto gpu_mode = config.vulkan_mode == core::VulkanMode::GpuBacked;
  const auto total_commands =
      gpu_mode ? kCommandTasks * 16U : std::max<std::uint64_t>(1, config.objects);
  std::vector<std::uint32_t> command_counts(kCommandTasks, 0);
  for (unsigned task = 0; task < kCommandTasks; ++task) {
    command_counts[task] = static_cast<std::uint32_t>(total_commands / kCommandTasks +
                                                      (task < total_commands % kCommandTasks));
  }

  const auto cpu_start = core::SteadyClock::now();
  runtime::parallel_for(*system_, kCommandTasks, 1, [&](std::size_t begin, std::size_t end) {
    for (auto task = begin; task < end; ++task) {
      if (gpu_mode) {
        context_->record_offscreen(task, command_counts[task],
                                   static_cast<std::uint32_t>(config.seed + task));
      } else {
        context_->record_no_submit(task, command_counts[task],
                                   static_cast<std::uint32_t>(config.seed + task));
      }
    }
  });
  if (gpu_mode) {
    context_->submit();
  }
  const auto cpu_end = core::SteadyClock::now();
  if (gpu_mode) {
    context_->wait();
  }

  std::uint64_t checksum = mix64(config.seed);
  std::uint64_t operations = 0;
  for (std::uint32_t count : command_counts) {
    checksum =
        mix64(checksum ^ ((static_cast<std::uint64_t>(count) << 32U) | (checksum & 0xffffffffU)));
    operations += count;
  }

  WorkloadOutput output;
  output.checksum = checksum;
  output.operations = operations;
  output.valid = true;
  output.measured_time_ms = core::milliseconds_since(cpu_start, cpu_end);
  output.vulkan = VulkanDiagnostics{.device_name = context_->device_name(),
                                    .driver_name = context_->driver_name(),
                                    .api_version = context_->api_version()};
  return output;
#endif
}

void VulkanWorkload::ensure_system(const WorkloadConfig& config) {
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
      .cpu_affinity = worker_affinity(config),
  });
}

auto VulkanWorkload::ensure_context(const WorkloadConfig&) -> bool {
#if defined(GAMEBENCH_HAS_VULKAN)
  if (context_) {
    return true;
  }
  if (!context_error_.empty()) {
    return false;
  }
  try {
    context_ = std::make_unique<renderer::VulkanContext>(kCommandTasks);
    return true;
  } catch (const std::exception& exception) {
    context_error_ = std::string{"Vulkan 不可用，工作负载已跳过: "} + exception.what();
    return false;
  }
#else
  context_error_ = "Vulkan 不可用，工作负载已跳过: 构建时未找到 Vulkan SDK";
  return false;
#endif
}

} // namespace gamebench::benchmark
