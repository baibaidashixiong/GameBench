#include "audio/dsp.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <vector>

TEST(AudioTest, RendersDeterministicStereoBuffers) {
  const auto first = gamebench::audio::render_source(42, 3, 256, 48000);
  const auto second = gamebench::audio::render_source(42, 3, 256, 48000);
  ASSERT_EQ(first.size(), 256U);
  ASSERT_EQ(second.size(), first.size());
  bool has_signal = false;
  for (std::size_t index = 0; index < first.size(); ++index) {
    EXPECT_FLOAT_EQ(first[index].left, second[index].left);
    EXPECT_FLOAT_EQ(first[index].right, second[index].right);
    has_signal = has_signal || std::abs(first[index].left) > 0.0F;
  }
  EXPECT_TRUE(has_signal);
}

TEST(AudioTest, MixesStereoSources) {
  const std::array<std::vector<gamebench::audio::StereoFrame>, 2> sources{{
      {{1.0F, 0.0F}, {0.5F, 0.5F}},
      {{0.0F, 1.0F}, {0.5F, 0.5F}},
  }};
  const auto mixed = gamebench::audio::mix_sources(sources, 2);
  ASSERT_EQ(mixed.size(), 2U);
  EXPECT_FLOAT_EQ(mixed[0].left, 0.5F);
  EXPECT_FLOAT_EQ(mixed[0].right, 0.5F);
  EXPECT_FLOAT_EQ(mixed[1].left, 0.5F);
  EXPECT_FLOAT_EQ(mixed[1].right, 0.5F);
}
