#include "core/version.hpp"

#include <gtest/gtest.h>

#include <string>

TEST(GameBenchInfrastructure, VersionIsNotEmpty) {
  EXPECT_FALSE(gamebench::core::kVersion.empty());
}

TEST(GameBenchInfrastructure, ProjectNameMatches) {
  EXPECT_EQ(gamebench::core::kProjectName, std::string{"GameBench"});
}

