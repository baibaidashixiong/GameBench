#include "core/timing.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace gamebench::core {
namespace {

[[nodiscard]] auto percentile(std::span<const double> sorted, double value) -> double {
  const auto rank = static_cast<std::size_t>(
      std::ceil(value * static_cast<double>(sorted.size())));
  const auto index = std::max<std::size_t>(1, rank) - 1;
  return sorted[index];
}

} // namespace

auto milliseconds_since(SteadyClock::time_point start, SteadyClock::time_point end) -> double {
  return std::chrono::duration<double, std::milli>(end - start).count();
}

auto calculate_statistics(std::span<const double> samples_ms) -> SampleStatistics {
  if (samples_ms.empty()) {
    throw std::invalid_argument("样本不能为空");
  }

  std::vector<double> sorted(samples_ms.begin(), samples_ms.end());
  std::ranges::sort(sorted);

  const double sum = std::accumulate(sorted.begin(), sorted.end(), 0.0);
  SampleStatistics result;
  result.mean_ms = sum / static_cast<double>(sorted.size());
  result.median_ms = percentile(sorted, 0.5);
  result.p95_ms = percentile(sorted, 0.95);
  result.p99_ms = percentile(sorted, 0.99);
  result.p999_ms = percentile(sorted, 0.999);
  result.maximum_ms = sorted.back();
  return result;
}

} // namespace gamebench::core

