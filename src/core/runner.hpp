#pragma once

#include "benchmark/frame_profile.hpp"
#include "benchmark/workload.hpp"
#include "core/config.hpp"
#include "core/timing.hpp"
#include "platform/cpu_topology.hpp"
#include "platform/system_info.hpp"
#include "profiler/hardware_counters.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace gamebench::core {

struct BenchmarkRun {
  unsigned thread_count{};
  unsigned worker_count{};
  std::string execution_mode;
  std::vector<double> samples_ms;
  std::vector<double> main_thread_cpu_ms;
  SampleStatistics statistics;
  double main_thread_time_ms{};
  double critical_path_time_ms{};
  std::optional<double> worker_thread_time_ms;
  std::optional<double> total_cpu_work_ms;
  std::uint64_t main_thread_cpu_migrations{};
  std::uint64_t operations{};
  std::uint64_t checksum{};
  bool valid{};
  bool skipped{};
  std::string validation_message;
  std::vector<std::string> warnings;
  std::optional<benchmark::SchedulingMetrics> scheduling;
  std::optional<benchmark::VulkanDiagnostics> vulkan;
  std::string simd_backend;
};

struct ThreadScaling {
  unsigned thread_count{};
  double speedup{};
  double efficiency{};
};

struct BenchmarkResult {
  std::string benchmark_id;
  std::string benchmark_version;
  std::string backend{"synthetic"};
  RunConfig config;
  platform::SystemInfo system;
  platform::CpuTopology topology;
  std::optional<profiler::HardwareCounterResult> hardware_counters;
  std::optional<benchmark::FrameProfile> frame_profile;
  std::vector<BenchmarkRun> runs;
  std::vector<ThreadScaling> thread_scaling;
  bool valid{};
};

class BenchmarkRunner final {
public:
  [[nodiscard]] auto run(benchmark::Workload& workload, const RunConfig& config) -> BenchmarkResult;

private:
  static void execute_iterations(benchmark::Workload& workload,
                                 const benchmark::WorkloadConfig& workload_config,
                                 std::uint64_t iterations, bool measure, BenchmarkRun& run,
                                 std::vector<double>* worker_busy_samples = nullptr,
                                 std::vector<double>* total_cpu_work_samples = nullptr);
};

} // namespace gamebench::core
