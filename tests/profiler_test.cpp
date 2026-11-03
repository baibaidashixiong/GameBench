#include "profiler/hardware_counters.hpp"

#include <gtest/gtest.h>

TEST(ProfilerTest, DisabledSessionDoesNotAccessPerf) {
  gamebench::profiler::HardwareCounterSession session(false);
  const auto result = session.stop();
  EXPECT_FALSE(result.requested);
  EXPECT_FALSE(result.available);
}

TEST(ProfilerTest, RequestedSessionDegradesWhenPerfIsUnavailable) {
  gamebench::profiler::HardwareCounterSession session(true);
  const auto result = session.stop();
  EXPECT_TRUE(result.requested);
  if (!result.available) {
    EXPECT_FALSE(result.message.empty());
  }
}
