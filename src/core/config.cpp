#include "core/config.hpp"

namespace gamebench::core {

auto to_string(RunMode mode) -> std::string_view {
  switch (mode) {
  case RunMode::Reproducible:
    return "reproducible";
  case RunMode::Realistic:
    return "realistic";
  }
  return "unknown";
}

auto to_string(SchedulerKind scheduler) -> std::string_view {
  switch (scheduler) {
  case SchedulerKind::GlobalQueue:
    return "global-queue";
  case SchedulerKind::StaticPartitioning:
    return "static-partitioning";
  case SchedulerKind::WorkStealing:
    return "work-stealing";
  }
  return "unknown";
}

auto to_string(ExportFormat format) -> std::string_view {
  switch (format) {
  case ExportFormat::Json:
    return "json";
  case ExportFormat::Csv:
    return "csv";
  }
  return "unknown";
}

auto to_string(DataLayout layout) -> std::string_view {
  switch (layout) {
  case DataLayout::AoS:
    return "aos";
  case DataLayout::SoA:
    return "soa";
  case DataLayout::AoSoA:
    return "aosoa";
  }
  return "unknown";
}

auto to_string(MemoryPattern pattern) -> std::string_view {
  switch (pattern) {
  case MemoryPattern::Sequential:
    return "sequential";
  case MemoryPattern::Random:
    return "random";
  case MemoryPattern::PointerChasing:
    return "pointer-chasing";
  case MemoryPattern::Strided:
    return "strided";
  }
  return "unknown";
}

auto to_string(VulkanMode mode) -> std::string_view {
  switch (mode) {
  case VulkanMode::NoSubmit:
    return "no-submit";
  case VulkanMode::GpuBacked:
    return "gpu-backed";
  }
  return "unknown";
}

} // namespace gamebench::core
