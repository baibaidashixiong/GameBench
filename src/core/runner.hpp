#pragma once

#include "benchmark/workload.hpp"
#include "core/config.hpp"
#include "core/timing.hpp"
#include "platform/system_info.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace gamebench::core {

struct BenchmarkRun {
  unsigned thread_count{};
  std::vector<double> samples_ms;
  SampleStatistics statistics;
  std::uint64_t operations{};
  std::uint64_t checksum{};
  bool valid{};
  bool skipped{};
  std::string validation_message;
  std::optional<benchmark::SchedulingMetrics> scheduling;
  std::optional<benchmark::VulkanDiagnostics> vulkan;
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
                                 std::uint64_t iterations, bool measure, BenchmarkRun& run);
};

} // namespace gamebench::core
