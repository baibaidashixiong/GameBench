#include "core/cli.hpp"
#include "core/config.hpp"

#include <gtest/gtest.h>

#include <array>
#include <stdexcept>
#include <string_view>

TEST(CliTest, ParsesRunConfiguration) {
  constexpr std::array arguments{
      std::string_view{"--workload"},    std::string_view{"integer"},
      std::string_view{"--threads"},     std::string_view{"1,2,4"},
      std::string_view{"--seed"},        std::string_view{"99"},
      std::string_view{"--objects"},     std::string_view{"4096"},
      std::string_view{"--warmup"},      std::string_view{"2"},
      std::string_view{"--repetitions"}, std::string_view{"5"},
  };

  const auto config = gamebench::core::parse_run_config(arguments);
  EXPECT_EQ(config.workload, "integer");
  ASSERT_EQ(config.thread_counts.size(), 3U);
  EXPECT_EQ(config.thread_counts[2], 4U);
  EXPECT_EQ(config.seed, 99U);
  EXPECT_EQ(config.objects, 4096U);
  EXPECT_EQ(config.warmup_iterations, 2U);
  EXPECT_EQ(config.repetitions, 5U);
}

TEST(CliTest, RejectsUnknownOptions) {
  constexpr std::array arguments{std::string_view{"--unknown"}};
  EXPECT_THROW((void)gamebench::core::parse_run_config(arguments), std::invalid_argument);
}
