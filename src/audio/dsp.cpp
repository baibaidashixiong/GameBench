#include "audio/dsp.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace gamebench::audio {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr std::array<float, 8> kFirCoefficients{0.05F, 0.10F, 0.15F, 0.20F,
                                                0.20F, 0.15F, 0.10F, 0.05F};

[[nodiscard]] constexpr auto mix64(std::uint64_t value) -> std::uint64_t {
  value ^= value >> 30U;
  value *= 0xbf58476d1ce4e5b9ULL;
  value ^= value >> 27U;
  value *= 0x94d049bb133111ebULL;
  value ^= value >> 31U;
  return value;
}

[[nodiscard]] auto fir(const std::vector<float>& input, std::vector<float>& output) {
  output.assign(input.size(), 0.0F);
  for (std::size_t index = 0; index < input.size(); ++index) {
    float value = 0.0F;
    for (std::size_t tap = 0; tap < kFirCoefficients.size() && tap <= index; ++tap) {
      value += input[index - tap] * kFirCoefficients[tap];
    }
    output[index] = value;
  }
}

} // namespace

auto render_source(std::uint64_t seed, std::size_t source_index, std::size_t frame_count,
                   std::uint32_t sample_rate) -> std::vector<StereoFrame> {
  if (frame_count == 0 || sample_rate == 0) {
    return {};
  }
  const auto state = mix64(seed + source_index * 0x9e3779b97f4a7c15ULL);
  constexpr std::size_t channels = 4;
  const auto oversampled_count = frame_count * 2U;
  std::vector<float> generated(oversampled_count * channels);
  for (std::size_t frame = 0; frame < oversampled_count; ++frame) {
    const auto time = static_cast<double>(frame) / (static_cast<double>(sample_rate) * 2.0);
    for (std::size_t channel = 0; channel < channels; ++channel) {
      const auto frequency = 110.0 + static_cast<double>((state >> (channel * 8U)) & 0xffU) * 3.0;
      const auto phase = static_cast<double>(state & 0xffffU) / 65535.0 * 2.0 * kPi;
      const auto fundamental = std::sin(2.0 * kPi * frequency * time + phase);
      const auto harmonic = std::sin(2.0 * kPi * frequency * 2.0 * time + phase) * 0.25;
      generated[frame * channels + channel] = static_cast<float>((fundamental + harmonic) * 0.12);
    }
  }

  std::vector<float> filtered(oversampled_count);
  std::vector<float> scratch(oversampled_count);
  std::vector<StereoFrame> result(frame_count);
  std::array<float, channels> reverb_state{};
  for (std::size_t channel = 0; channel < channels; ++channel) {
    for (std::size_t frame = 0; frame < oversampled_count; ++frame) {
      scratch[frame] = generated[frame * channels + channel];
    }
    fir(scratch, filtered);
    for (std::size_t frame = 0; frame < frame_count; ++frame) {
      const auto source_position = static_cast<double>(frame) * 2.0;
      const auto lower = static_cast<std::size_t>(source_position);
      const auto upper = std::min(lower + 1U, filtered.size() - 1U);
      const auto fraction = static_cast<float>(source_position - static_cast<double>(lower));
      const auto sample = filtered[lower] * (1.0F - fraction) + filtered[upper] * fraction;
      if (channel == 0 || channel == 2) {
        result[frame].left += sample * 0.5F + reverb_state[channel] * 0.02F;
      }
      if (channel == 1 || channel == 3) {
        result[frame].right += sample * 0.5F + reverb_state[channel] * 0.02F;
      }
      reverb_state[channel] = reverb_state[channel] * 0.97F + sample * 0.03F;
    }
  }

  constexpr std::size_t delay_frames = 31;
  for (std::size_t frame = delay_frames; frame < result.size(); ++frame) {
    result[frame].left += result[frame - delay_frames].right * 0.12F;
    result[frame].right += result[frame - delay_frames].left * 0.12F;
  }
  return result;
}

auto mix_sources(std::span<const std::vector<StereoFrame>> sources, std::size_t frame_count)
    -> std::vector<StereoFrame> {
  std::vector<StereoFrame> mixed(frame_count);
  for (const auto& source : sources) {
    const auto count = std::min(frame_count, source.size());
    for (std::size_t frame = 0; frame < count; ++frame) {
      mixed[frame].left += source[frame].left;
      mixed[frame].right += source[frame].right;
    }
  }
  const auto normalization = sources.empty() ? 1.0F : 1.0F / static_cast<float>(sources.size());
  for (auto& frame : mixed) {
    frame.left *= normalization;
    frame.right *= normalization;
  }
  return mixed;
}

} // namespace gamebench::audio
