#pragma once

#include "core/timing.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace gamebench::benchmark {

enum class EngineAvailability { Available, DetectedOnly, MissingDependency, NotImplemented };

struct EngineAdapterInfo {
  std::string id;
  std::string display_name;
  std::string integration_version;
  EngineAvailability availability{EngineAvailability::NotImplemented};
  std::string message;
};

struct EngineBenchmarkRequest {
  std::uint64_t seed{};
  std::uint64_t objects{};
  unsigned thread_count{1};
  std::uint64_t warmup_frames{};
  std::uint64_t measured_frames{};
  std::filesystem::path project_path;
};

struct EngineBenchmarkOutput {
  std::vector<double> frame_times_ms;
  std::uint64_t checksum{};
  bool valid{};
  std::string validation_message;
};

class EngineAdapter {
public:
  virtual ~EngineAdapter() = default;

  [[nodiscard]] virtual auto info() const -> EngineAdapterInfo = 0;
  [[nodiscard]] virtual auto run(const EngineBenchmarkRequest& request)
      -> EngineBenchmarkOutput = 0;
};

[[nodiscard]] auto detect_godot_adapter() -> EngineAdapterInfo;
[[nodiscard]] auto detect_source2_adapter() -> EngineAdapterInfo;
[[nodiscard]] auto planned_engine_adapters() -> std::vector<EngineAdapterInfo>;

} // namespace gamebench::benchmark
