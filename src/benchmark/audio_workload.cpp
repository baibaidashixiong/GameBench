#include "benchmark/audio_workload.hpp"

#include "audio/dsp.hpp"
#include "runtime/job_system.hpp"
#include "runtime/parallel.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <vector>

namespace gamebench::benchmark {
namespace {

[[nodiscard]] constexpr auto mix64(std::uint64_t value) -> std::uint64_t {
  value ^= value >> 30U;
  value *= 0xbf58476d1ce4e5b9ULL;
  value ^= value >> 27U;
  value *= 0x94d049bb133111ebULL;
  value ^= value >> 31U;
  return value;
}

} // namespace

AudioWorkload::AudioWorkload() = default;
AudioWorkload::~AudioWorkload() = default;
auto AudioWorkload::id() const -> std::string_view {
  return "audio";
}
auto AudioWorkload::version() const -> std::string_view {
  return "1";
}
auto AudioWorkload::category() const -> std::string_view {
  return "synthetic";
}
auto AudioWorkload::description() const -> std::string_view {
  return "多音源 FIR、重采样、Reverb 与 Stereo Mixing";
}

auto AudioWorkload::execute(const WorkloadConfig& config) -> WorkloadOutput {
  ensure_system(config);
  std::vector<std::vector<audio::StereoFrame>> sources(static_cast<std::size_t>(config.objects));
  const auto grain = std::max<std::size_t>(8, static_cast<std::size_t>(config.objects) /
                                                  std::max(1U, config.thread_count * 8U));
  runtime::parallel_for(*system_, sources.size(), grain, [&](std::size_t begin, std::size_t end) {
    for (auto source = begin; source < end; ++source) {
      sources[source] =
          audio::render_source(config.seed, source, config.audio_buffer_size, config.sample_rate);
    }
  });
  const auto mixed = audio::mix_sources(sources, config.audio_buffer_size);
  std::uint64_t checksum = mix64(config.seed);
  for (const auto& frame : mixed) {
    checksum = mix64(checksum ^ std::bit_cast<std::uint32_t>(frame.left));
    checksum = mix64(checksum ^ std::bit_cast<std::uint32_t>(frame.right));
  }

  WorkloadOutput output;
  output.checksum = checksum;
  output.operations = config.objects * config.audio_buffer_size * 8U;
  output.valid = !mixed.empty() && checksum != 0;
  if (!output.valid) {
    output.validation_message = "Audio DSP 混音校验失败";
  }
  set_job_execution_metadata(output, config);
  return output;
}

void AudioWorkload::ensure_system(const WorkloadConfig& config) {
  if (system_ && prepared_threads_ == config.thread_count &&
      prepared_scheduler_ == config.scheduler) {
    return;
  }
  system_.reset();
  prepared_threads_ = config.thread_count;
  prepared_scheduler_ = config.scheduler;
  system_ = std::make_unique<runtime::JobSystem>(runtime::JobSystemConfig{
      .worker_count = job_worker_count(config),
      .main_thread_participates = config.thread_count > 0,
      .scheduler = config.scheduler,
      .cpu_affinity = worker_affinity(config),
  });
}

} // namespace gamebench::benchmark
