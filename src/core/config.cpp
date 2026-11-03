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

} // namespace gamebench::core
