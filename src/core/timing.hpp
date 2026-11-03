#pragma once

#include <chrono>
#include <span>

namespace gamebench::core {

using SteadyClock = std::chrono::steady_clock;

struct SampleStatistics {
  double mean_ms{};
  double median_ms{};
  double p95_ms{};
  double p99_ms{};
  double p999_ms{};
  double maximum_ms{};
};

[[nodiscard]] auto milliseconds_since(SteadyClock::time_point start, SteadyClock::time_point end)
    -> double;
[[nodiscard]] auto calculate_statistics(std::span<const double> samples_ms) -> SampleStatistics;

} // namespace gamebench::core
