#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace gamebench::core {

enum class RunMode { Reproducible, Realistic };
enum class SchedulerKind { GlobalQueue, StaticPartitioning, WorkStealing };
enum class ExportFormat { Json, Csv };
enum class DataLayout { AoS, SoA, AoSoA };
enum class MemoryPattern { Sequential, Random, PointerChasing, Strided };
enum class VulkanMode { NoSubmit, GpuBacked };

struct RunConfig {
  std::string workload{"game-process-mix"};
  std::string profile{"source2-like"};
  bool suite_all{false};
  std::vector<unsigned> thread_counts;
  RunMode mode{RunMode::Reproducible};
  SchedulerKind scheduler{SchedulerKind::WorkStealing};
  DataLayout layout{DataLayout::SoA};
  MemoryPattern memory_pattern{MemoryPattern::Sequential};
  VulkanMode vulkan_mode{VulkanMode::NoSubmit};
  std::uint32_t audio_buffer_size{1024};
  std::uint32_t sample_rate{48000};
  std::vector<unsigned> cpu_affinity;
  bool hardware_counters{};
  std::uint64_t seed{1337};
  std::uint64_t objects{65536};
  std::uint64_t iterations{1};
  std::uint64_t warmup_iterations{60};
  std::uint64_t repetitions{300};
  std::uint64_t working_set_bytes{16ULL * 1024ULL * 1024ULL};
  ExportFormat format{ExportFormat::Json};
  std::filesystem::path output_path;
};

[[nodiscard]] auto to_string(RunMode mode) -> std::string_view;
[[nodiscard]] auto to_string(SchedulerKind scheduler) -> std::string_view;
[[nodiscard]] auto to_string(ExportFormat format) -> std::string_view;
[[nodiscard]] auto to_string(DataLayout layout) -> std::string_view;
[[nodiscard]] auto to_string(MemoryPattern pattern) -> std::string_view;
[[nodiscard]] auto to_string(VulkanMode mode) -> std::string_view;

} // namespace gamebench::core
