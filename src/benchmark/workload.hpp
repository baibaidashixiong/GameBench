#pragma once

#include "core/config.hpp"
#include "core/timing.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace gamebench::benchmark {

struct WorkloadConfig {
  unsigned thread_count{1};
  std::uint64_t seed{};
  std::uint64_t objects{};
  std::uint64_t iterations{};
  core::SchedulerKind scheduler{core::SchedulerKind::WorkStealing};
  core::DataLayout layout{core::DataLayout::SoA};
  core::MemoryPattern memory_pattern{core::MemoryPattern::Sequential};
  std::uint64_t working_set_bytes{};
  core::VulkanMode vulkan_mode{core::VulkanMode::NoSubmit};
};

struct SchedulingMetrics {
  std::uint64_t submitted_jobs{};
  std::uint64_t completed_jobs{};
  std::uint64_t stolen_jobs{};
  std::uint64_t main_thread_jobs{};
  std::uint64_t exceptions{};
  double worker_busy_ms{};
};

struct VulkanDiagnostics {
  std::string device_name;
  std::string driver_name;
  std::uint32_t api_version{};
};

struct WorkloadOutput {
  std::uint64_t checksum{};
  std::uint64_t operations{};
  bool valid{};
  bool skipped{};
  std::string validation_message;
  std::optional<SchedulingMetrics> scheduling;
  std::optional<VulkanDiagnostics> vulkan;
  std::optional<double> measured_time_ms;
};

class Workload {
public:
  virtual ~Workload() = default;

  [[nodiscard]] virtual auto id() const -> std::string_view = 0;
  [[nodiscard]] virtual auto version() const -> std::string_view = 0;
  [[nodiscard]] virtual auto category() const -> std::string_view = 0;
  [[nodiscard]] virtual auto description() const -> std::string_view = 0;
  [[nodiscard]] virtual auto execute(const WorkloadConfig& config) -> WorkloadOutput = 0;
};

} // namespace gamebench::benchmark
