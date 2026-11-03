#pragma once

#include <cstdint>
#include <string>

namespace gamebench::profiler {

struct HardwareCounterResult {
  bool requested{};
  bool available{};
  std::uint64_t cycles{};
  std::uint64_t instructions{};
  std::uint64_t branch_misses{};
  std::uint64_t cache_misses{};
  std::uint64_t context_switches{};
  double ipc{};
  std::string scope{"main-thread"};
  std::string message;
};

class HardwareCounterSession final {
public:
  explicit HardwareCounterSession(bool enabled);
  ~HardwareCounterSession();

  HardwareCounterSession(const HardwareCounterSession&) = delete;
  auto operator=(const HardwareCounterSession&) -> HardwareCounterSession& = delete;
  HardwareCounterSession(HardwareCounterSession&&) = delete;
  auto operator=(HardwareCounterSession&&) -> HardwareCounterSession& = delete;

  [[nodiscard]] auto stop() -> HardwareCounterResult;

private:
  struct Impl;
  Impl* impl_{};
};

} // namespace gamebench::profiler
