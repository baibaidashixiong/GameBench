#include "platform/system_info.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <thread>

#include <sys/utsname.h>

namespace gamebench::platform {
namespace {

[[nodiscard]] auto read_first_line(const char* path) -> std::string {
  std::ifstream input(path);
  std::string line;
  std::getline(input, line);
  return line;
}

[[nodiscard]] auto cpuinfo_value(std::string_view key) -> std::string {
  std::ifstream input("/proc/cpuinfo");
  std::string line;
  while (std::getline(input, line)) {
    const auto delimiter = line.find(':');
    if (delimiter == std::string::npos) {
      continue;
    }
    auto field = std::string_view(line).substr(0, delimiter);
    while (!field.empty() && (field.back() == ' ' || field.back() == '\t')) {
      field.remove_suffix(1);
    }
    if (field != key) {
      continue;
    }
    auto value = std::string_view(line).substr(delimiter + 1);
    while (!value.empty() && value.front() == ' ') {
      value.remove_prefix(1);
    }
    return std::string(value);
  }
  return "Unknown";
}

[[nodiscard]] auto split_features(const std::string& flags) -> std::vector<std::string> {
  std::vector<std::string> result;
  std::istringstream stream(flags);
  std::string feature;
  while (stream >> feature) {
    result.push_back(std::move(feature));
  }
  return result;
}

} // namespace

auto collect_system_info() -> SystemInfo {
  SystemInfo result;
  result.cpu_vendor = cpuinfo_value("vendor_id");
  result.cpu_model = cpuinfo_value("model name");
  result.logical_processors = std::thread::hardware_concurrency();
  result.cpu_features = split_features(cpuinfo_value("flags"));

  utsname system_name{};
  if (uname(&system_name) == 0) {
    result.operating_system = system_name.sysname;
    result.kernel_release = system_name.release;
  }

#if defined(__clang__)
  result.compiler = "Clang";
  result.compiler_version = __clang_version__;
#elif defined(__GNUC__)
  result.compiler = "GCC";
  result.compiler_version = __VERSION__;
#else
  result.compiler = "Unknown";
  result.compiler_version = "Unknown";
#endif

#if defined(NDEBUG)
  result.build_type = "Release";
#else
  result.build_type = "Debug";
#endif

  const auto governor = read_first_line("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");
  if (!governor.empty()) {
    result.cpu_governor = governor;
  }
  return result;
}

} // namespace gamebench::platform

