#include "benchmark/process_worker.hpp"

#include "benchmark/ai_workload.hpp"
#include "benchmark/audio_workload.hpp"
#include "benchmark/composite_frame_workload.hpp"
#include "benchmark/physics_workload.hpp"
#include "benchmark/process_protocol.hpp"
#include "benchmark/scene_workload.hpp"
#include "benchmark/workload.hpp"
#include "platform/cpu_affinity.hpp"

#include <cerrno>
#include <charconv>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

namespace gamebench::benchmark {
namespace {

using process_protocol::Command;
using process_protocol::Reply;
using process_protocol::Request;
using process_protocol::Role;

struct WorkerArguments {
  Role role{};
  unsigned thread_count{1};
  std::uint64_t seed{};
  int control_fd{process_protocol::kControlFd};
  int result_fd{process_protocol::kResultFd};
  std::vector<unsigned> cpu_affinity;
};

template<typename Integer>
[[nodiscard]] auto parse_integer(std::string_view value, std::string_view name) -> Integer {
  Integer result{};
  const auto* begin = value.data();
  const auto* end = begin + value.size();
  const auto [position, error] = std::from_chars(begin, end, result);
  if (error != std::errc{} || position != end) {
    throw std::invalid_argument("进程 Worker 参数无效: " + std::string{name});
  }
  return result;
}

[[nodiscard]] auto parse_affinity(std::string_view value) -> std::vector<unsigned> {
  std::vector<unsigned> cpus;
  while (!value.empty()) {
    const auto separator = value.find(',');
    const auto token = value.substr(0, separator);
    if (token.empty()) {
      throw std::invalid_argument("进程 Worker Affinity 包含空值");
    }
    cpus.push_back(parse_integer<unsigned>(token, "--affinity"));
    if (separator == std::string_view::npos) {
      break;
    }
    value.remove_prefix(separator + 1U);
  }
  return cpus;
}

[[nodiscard]] auto parse_arguments(std::span<const std::string_view> arguments) -> WorkerArguments {
  WorkerArguments result;
  bool role_seen = false;
  for (std::size_t index = 0; index < arguments.size(); ++index) {
    const auto option = arguments[index];
    const auto value = [&]() -> std::string_view {
      if (index + 1U >= arguments.size()) {
        throw std::invalid_argument("进程 Worker 参数缺少值: " + std::string{option});
      }
      ++index;
      return arguments[index];
    };
    if (option == "--role") {
      const auto role = value();
      if (role == "main-game") {
        result.role = Role::MainGame;
      } else if (role == "scene-aux") {
        result.role = Role::SceneAuxiliary;
      } else if (role == "physics-aux") {
        result.role = Role::PhysicsAuxiliary;
      } else if (role == "ai-aux") {
        result.role = Role::AiAuxiliary;
      } else if (role == "audio-aux") {
        result.role = Role::AudioAuxiliary;
      } else {
        throw std::invalid_argument("未知进程 Worker 角色: " + std::string{role});
      }
      role_seen = true;
    } else if (option == "--threads") {
      result.thread_count = parse_integer<unsigned>(value(), option);
    } else if (option == "--seed") {
      result.seed = parse_integer<std::uint64_t>(value(), option);
    } else if (option == "--control-fd") {
      result.control_fd = parse_integer<int>(value(), option);
    } else if (option == "--result-fd") {
      result.result_fd = parse_integer<int>(value(), option);
    } else if (option == "--affinity") {
      result.cpu_affinity = parse_affinity(value());
    } else {
      throw std::invalid_argument("未知进程 Worker 参数: " + std::string{option});
    }
  }
  if (!role_seen || result.thread_count == 0 || result.control_fd < 0 || result.result_fd < 0) {
    throw std::invalid_argument("进程 Worker 参数不完整");
  }
  return result;
}

[[nodiscard]] auto read_exact(int fd, void* destination, std::size_t size) -> bool {
  auto* bytes = static_cast<std::byte*>(destination);
  std::size_t offset = 0;
  while (offset < size) {
    const auto count = read(fd, bytes + offset, size - offset);
    if (count > 0) {
      offset += static_cast<std::size_t>(count);
      continue;
    }
    if (count < 0 && errno == EINTR) {
      continue;
    }
    return false;
  }
  return true;
}

template<typename Message>
[[nodiscard]] auto write_message(int fd, const Message& message) -> bool {
  const auto* bytes = reinterpret_cast<const std::byte*>(&message);
  std::size_t offset = 0;
  while (offset < sizeof(Message)) {
    const auto count = write(fd, bytes + offset, sizeof(Message) - offset);
    if (count > 0) {
      offset += static_cast<std::size_t>(count);
      continue;
    }
    if (count < 0 && errno == EINTR) {
      continue;
    }
    return false;
  }
  return true;
}

[[nodiscard]] auto process_cpu_time_ms() -> double {
  timespec value{};
  if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &value) != 0) {
    return 0.0;
  }
  return static_cast<double>(value.tv_sec) * 1000.0 +
         static_cast<double>(value.tv_nsec) / 1'000'000.0;
}

[[nodiscard]] auto peak_rss_kb() -> double {
  rusage usage{};
  if (getrusage(RUSAGE_SELF, &usage) != 0) {
    return 0.0;
  }
  return static_cast<double>(usage.ru_maxrss);
}

[[nodiscard]] auto make_workload(Role role) -> std::unique_ptr<Workload> {
  switch (role) {
  case Role::MainGame:
    return std::make_unique<CompositeFrameWorkload>();
  case Role::SceneAuxiliary:
    return std::make_unique<SceneWorkload>();
  case Role::PhysicsAuxiliary:
    return std::make_unique<PhysicsWorkload>();
  case Role::AiAuxiliary:
    return std::make_unique<AiWorkload>();
  case Role::AudioAuxiliary:
    return std::make_unique<AudioWorkload>();
  }
  throw std::invalid_argument("未知进程 Worker 角色");
}

[[nodiscard]] auto make_workload_config(const WorkerArguments& arguments) -> WorkloadConfig {
  WorkloadConfig config;
  config.thread_count = arguments.thread_count;
  config.seed = arguments.seed;
  config.scheduler = core::SchedulerKind::WorkStealing;
  config.layout = core::DataLayout::SoA;
  config.memory_pattern = core::MemoryPattern::Sequential;
  config.vulkan_mode = core::VulkanMode::NoSubmit;
  config.audio_buffer_size = 1024;
  config.sample_rate = 48000;
  config.cpu_affinity = arguments.cpu_affinity;
  config.enable_vulkan = false;
  switch (arguments.role) {
  case Role::MainGame:
    config.profile_id = "source2-like";
    break;
  case Role::SceneAuxiliary:
    config.objects = 18000;
    break;
  case Role::PhysicsAuxiliary:
    config.objects = 3072;
    break;
  case Role::AiAuxiliary:
    config.objects = 96;
    break;
  case Role::AudioAuxiliary:
    config.objects = 32;
    break;
  }
  return config;
}

auto worker_main(const WorkerArguments& arguments) -> int {
  (void)signal(SIGPIPE, SIG_DFL);
  const std::vector<unsigned> main_affinity =
      arguments.cpu_affinity.empty() ? std::vector<unsigned>{}
                                     : std::vector<unsigned>{arguments.cpu_affinity.front()};
  platform::ScopedThreadAffinity affinity(main_affinity);
  auto workload = make_workload(arguments.role);
  const auto workload_config = make_workload_config(arguments);
  const process_protocol::ReadyMessage ready{
      .magic = process_protocol::kMagic,
      .pid = static_cast<std::uint64_t>(getpid()),
      .role = arguments.role,
  };
  if (!write_message(arguments.result_fd, ready)) {
    return 1;
  }

  while (true) {
    Request request;
    if (!read_exact(arguments.control_fd, &request, sizeof(request))) {
      return 1;
    }
    if (request.magic != process_protocol::kMagic) {
      return 1;
    }
    if (request.command == Command::Stop) {
      return 0;
    }
    if (request.command != Command::RunFrame) {
      return 1;
    }

    Reply reply;
    const auto cpu_start = process_cpu_time_ms();
    const auto start = std::chrono::steady_clock::now();
    try {
      const auto output = workload->execute(workload_config);
      reply.checksum = output.checksum;
      reply.operations = output.operations;
      reply.valid = output.valid && !output.skipped;
      if (!output.simd_backend.empty()) {
        std::snprintf(reply.simd_backend.data(), reply.simd_backend.size(), "%s",
                      output.simd_backend.c_str());
      }
      if (!reply.valid) {
        std::snprintf(reply.error.data(), reply.error.size(), "%s",
                      output.validation_message.empty() ? "子进程工作负载校验失败"
                                                        : output.validation_message.c_str());
      }
    } catch (const std::exception& exception) {
      reply.valid = false;
      std::snprintf(reply.error.data(), reply.error.size(), "%s", exception.what());
    }
    const auto end = std::chrono::steady_clock::now();
    reply.workload_time_ms = std::chrono::duration<double, std::milli>(end - start).count();
    reply.process_cpu_time_ms = process_cpu_time_ms() - cpu_start;
    reply.peak_rss_kb = peak_rss_kb();
    if (!write_message(arguments.result_fd, reply)) {
      return 1;
    }
  }
}

} // namespace

auto run_process_worker(std::span<const std::string_view> arguments) -> int {
  try {
    return worker_main(parse_arguments(arguments));
  } catch (const std::exception& exception) {
    const std::string message = std::string{"进程 Worker 启动失败: "} + exception.what() + '\n';
    (void)write(STDERR_FILENO, message.data(), message.size());
    return 1;
  }
}

} // namespace gamebench::benchmark
