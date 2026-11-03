#pragma once

#include "core/timing.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace gamebench::benchmark {

struct WorkloadConfig {
  unsigned thread_count{1};
  std::uint64_t seed{};
  std::uint64_t objects{};
  std::uint64_t iterations{};
};

struct WorkloadOutput {
  std::uint64_t checksum{};
  std::uint64_t operations{};
  bool valid{};
  std::string validation_message;
};

class Workload {
public:
  virtual ~Workload() = default;

  [[nodiscard]] virtual auto id() const -> std::string_view = 0;
  [[nodiscard]] virtual auto version() const -> std::string_view = 0;
  [[nodiscard]] virtual auto category() const -> std::string_view = 0;
  [[nodiscard]] virtual auto description() const -> std::string_view = 0;
  [[nodiscard]] virtual auto execute(const WorkloadConfig& config) -> WorkloadOutput = 0;
};

} // namespace gamebench::benchmark

