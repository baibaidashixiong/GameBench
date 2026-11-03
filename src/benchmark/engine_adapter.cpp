#include "benchmark/engine_adapter.hpp"

#include <cstdlib>
#include <string>
#include <vector>

#include <unistd.h>

namespace gamebench::benchmark {
namespace {

[[nodiscard]] auto executable_exists(const std::filesystem::path& path) -> bool {
  return !path.empty() && access(path.c_str(), X_OK) == 0;
}

} // namespace

auto detect_godot_adapter() -> EngineAdapterInfo {
  EngineAdapterInfo info{.id = "godot",
                         .display_name = "Godot 4",
                         .integration_version = "contract-v1",
                         .message = {}};
  const char* environment_path = std::getenv("GODOT_BIN");
  if (environment_path != nullptr && executable_exists(environment_path)) {
    info.availability = EngineAvailability::Available;
    info.message = "检测到 GODOT_BIN=" + std::string{environment_path};
    return info;
  }
  for (const auto* candidate : {"godot4", "godot"}) {
    if (const char* path = std::getenv("PATH"); path != nullptr) {
      std::string search{path};
      std::size_t start = 0;
      while (start <= search.size()) {
        const auto delimiter = search.find(':', start);
        const auto end = delimiter == std::string::npos ? search.size() : delimiter;
        const auto directory = search.substr(start, end - start);
        if (!directory.empty() && executable_exists(std::filesystem::path{directory} / candidate)) {
          info.availability = EngineAvailability::Available;
          info.message =
              "检测到 Godot 可执行文件: " + (std::filesystem::path{directory} / candidate).string();
          return info;
        }
        if (delimiter == std::string::npos) {
          break;
        }
        start = delimiter + 1;
      }
    }
  }
  info.availability = EngineAvailability::MissingDependency;
  info.message = "未检测到 godot4/godot；设置 GODOT_BIN 后可继续集成";
  return info;
}

auto planned_engine_adapters() -> std::vector<EngineAdapterInfo> {
  return {
      detect_godot_adapter(),
      {.id = "unreal",
       .display_name = "Unreal Engine 5",
       .integration_version = "planned",
       .availability = EngineAvailability::NotImplemented,
       .message = "接口已预留，执行适配器尚未实现"},
      {.id = "o3de",
       .display_name = "Open 3D Engine",
       .integration_version = "planned",
       .availability = EngineAvailability::NotImplemented,
       .message = "接口已预留，执行适配器尚未实现"},
  };
}

} // namespace gamebench::benchmark
