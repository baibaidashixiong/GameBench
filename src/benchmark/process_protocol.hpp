#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace gamebench::benchmark::process_protocol {

inline constexpr std::uint64_t kMagic = 0x474250524f433031ULL;
inline constexpr int kControlFd = 10;
inline constexpr int kResultFd = 11;

enum class Role : std::uint32_t {
  MainGame = 0,
  SceneAuxiliary = 1,
  PhysicsAuxiliary = 2,
  AiAuxiliary = 3,
  AudioAuxiliary = 4,
};
enum class Command : std::uint32_t { Stop = 0, RunFrame = 1 };

struct ReadyMessage {
  std::uint64_t magic{kMagic};
  std::uint64_t pid{};
  Role role{};
};

struct Request {
  std::uint64_t magic{kMagic};
  Command command{Command::RunFrame};
  std::uint64_t frame_index{};
};

struct Reply {
  std::uint64_t magic{kMagic};
  std::uint64_t checksum{};
  std::uint64_t operations{};
  double workload_time_ms{};
  double process_cpu_time_ms{};
  double peak_rss_kb{};
  bool valid{};
  std::array<char, 128> error{};
  std::array<char, 16> simd_backend{};
};

static_assert(std::is_trivially_copyable_v<ReadyMessage>);
static_assert(std::is_trivially_copyable_v<Request>);
static_assert(std::is_trivially_copyable_v<Reply>);

} // namespace gamebench::benchmark::process_protocol
