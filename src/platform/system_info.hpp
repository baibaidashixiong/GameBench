#pragma once

#include <optional>
#include <string>
#include <vector>

namespace gamebench::platform {

struct SystemInfo {
  std::string cpu_vendor{"Unknown"};
  std::string cpu_model{"Unknown"};
  unsigned logical_processors{};
  std::string operating_system{"Linux"};
  std::string kernel_release{"Unknown"};
  std::string compiler{"Unknown"};
  std::string compiler_version{"Unknown"};
  std::string build_type{"Unknown"};
  std::string cpu_governor{"Unknown"};
  std::optional<double> current_frequency_mhz;
  std::optional<double> minimum_frequency_mhz;
  std::optional<double> maximum_frequency_mhz;
  std::optional<double> package_temperature_celsius;
  std::vector<std::string> cpu_features;
};

[[nodiscard]] auto collect_system_info() -> SystemInfo;

} // namespace gamebench::platform
