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

struct RunConfig {
  std::string workload{"integer"};
  bool suite_all{false};
  std::vector<unsigned> thread_counts{1};
  RunMode mode{RunMode::Reproducible};
  SchedulerKind scheduler{SchedulerKind::WorkStealing};
  std::uint64_t seed{1337};
  std::uint64_t objects{65536};
  std::uint64_t iterations{1};
  std::uint64_t warmup_iterations{3};
  std::uint64_t repetitions{10};
  ExportFormat format{ExportFormat::Json};
  std::filesystem::path output_path;
};

[[nodiscard]] auto to_string(RunMode mode) -> std::string_view;
[[nodiscard]] auto to_string(SchedulerKind scheduler) -> std::string_view;
[[nodiscard]] auto to_string(ExportFormat format) -> std::string_view;

} // namespace gamebench::core

