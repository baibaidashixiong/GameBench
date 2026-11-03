#include "core/timing.hpp"

#include <gtest/gtest.h>

#include <array>
#include <stdexcept>

TEST(TimingTest, CalculatesNearestRankStatistics) {
  constexpr std::array samples{1.0, 2.0, 3.0, 4.0, 5.0};
  const auto statistics = gamebench::core::calculate_statistics(samples);

  EXPECT_DOUBLE_EQ(statistics.mean_ms, 3.0);
  EXPECT_DOUBLE_EQ(statistics.median_ms, 3.0);
  EXPECT_DOUBLE_EQ(statistics.p95_ms, 5.0);
  EXPECT_DOUBLE_EQ(statistics.p99_ms, 5.0);
  EXPECT_DOUBLE_EQ(statistics.p999_ms, 5.0);
  EXPECT_DOUBLE_EQ(statistics.maximum_ms, 5.0);
}

TEST(TimingTest, RejectsEmptySamples) {
  EXPECT_THROW((void)gamebench::core::calculate_statistics({}), std::invalid_argument);
}
