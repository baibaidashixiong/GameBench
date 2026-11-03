#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace gamebench::audio {

struct StereoFrame {
  float left{};
  float right{};
};

[[nodiscard]] auto render_source(std::uint64_t seed, std::size_t source_index,
                                 std::size_t frame_count, std::uint32_t sample_rate)
    -> std::vector<StereoFrame>;
[[nodiscard]] auto mix_sources(std::span<const std::vector<StereoFrame>> sources,
                               std::size_t frame_count) -> std::vector<StereoFrame>;

} // namespace gamebench::audio
