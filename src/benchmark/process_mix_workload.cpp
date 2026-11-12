#include "benchmark/process_mix_workload.hpp"

#include "benchmark/process_protocol.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <memory>
#include <numeric>
#include <poll.h>
#include <spawn.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace gamebench::benchmark {
namespace {

using process_protocol::Command;
using process_protocol::ReadyMessage;
using process_protocol::Reply;
using process_protocol::Request;
using process_protocol::Role;

constexpr auto kFrameTimeout = std::chrono::seconds{15};
constexpr auto kReadyTimeout = std::chrono::seconds{10};
constexpr auto kShutdownTimeout = std::chrono::seconds{2};

[[nodiscard]] constexpr auto mix64(std::uint64_t value) -> std::uint64_t {
  value ^= value >> 30U;
  value *= 0xbf58476d1ce4e5b9ULL;
  value ^= value >> 27U;
  value *= 0x94d049bb133111ebULL;
  value ^= value >> 31U;
  return value;
}

[[nodiscard]] auto role_name(Role role) -> std::string_view {
  switch (role) {
  case Role::MainGame:
    return "main-game";
  case Role::SceneAuxiliary:
    return "scene-aux";
  case Role::PhysicsAuxiliary:
    return "physics-aux";
  case Role::AiAuxiliary:
    return "ai-aux";
  case Role::AudioAuxiliary:
    return "audio-aux";
  }
  return "unknown";
}

[[nodiscard]] auto affinity_string(const std::vector<unsigned>& affinity) -> std::string {
  std::string result;
  for (std::size_t index = 0; index < affinity.size(); ++index) {
    if (index != 0) {
      result += ',';
    }
    result += std::to_string(affinity[index]);
  }
  return result;
}

[[nodiscard]] auto next_available_fd(int start) -> int {
  for (int fd = start; fd < 1024; ++fd) {
    if (fcntl(fd, F_GETFD) < 0 && errno == EBADF) {
      return fd;
    }
  }
  throw std::runtime_error("无法分配子进程协议文件描述符");
}

[[nodiscard]] auto wait_readable(int fd, std::chrono::steady_clock::duration timeout) -> bool {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (true) {
    const auto remaining = deadline - std::chrono::steady_clock::now();
    if (remaining <= std::chrono::steady_clock::duration::zero()) {
      return false;
    }
    const auto remaining_ms =
        static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(remaining).count());
    pollfd descriptor{.fd = fd, .events = POLLIN, .revents = 0};
    const auto result = poll(&descriptor, 1, std::max(1, remaining_ms));
    if (result > 0) {
      return (descriptor.revents & POLLIN) != 0;
    }
    if (result == 0) {
      return false;
    }
    if (errno != EINTR) {
      return false;
    }
  }
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

class ChildProcess final {
public:
  ChildProcess(Role role, std::vector<unsigned> affinity)
      : role_(role), affinity_(std::move(affinity)) {}

  ChildProcess(const ChildProcess&) = delete;
  auto operator=(const ChildProcess&) -> ChildProcess& = delete;

  ~ChildProcess() {
    close_pipes();
  }

  void spawn(const std::filesystem::path& executable, unsigned thread_count, std::uint64_t seed) {
    int control_pipe[2]{};
    int result_pipe[2]{};
    if (pipe2(control_pipe, O_CLOEXEC) != 0) {
      throw std::runtime_error("创建进程间 Pipe 失败: " + std::string{std::strerror(errno)});
    }
    if (pipe2(result_pipe, O_CLOEXEC) != 0) {
      close(control_pipe[0]);
      close(control_pipe[1]);
      throw std::runtime_error("创建进程间 Pipe 失败: " + std::string{std::strerror(errno)});
    }
    control_fd_ = control_pipe[1];
    result_fd_ = result_pipe[0];
    const auto child_control_fd = next_available_fd(100);
    auto child_result_fd = next_available_fd(child_control_fd + 1);
    if (child_result_fd == child_control_fd) {
      child_result_fd = next_available_fd(child_result_fd + 1);
    }

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, control_pipe[0], child_control_fd);
    posix_spawn_file_actions_adddup2(&actions, result_pipe[1], child_result_fd);
    if (control_pipe[0] != child_control_fd) {
      posix_spawn_file_actions_addclose(&actions, control_pipe[0]);
    }
    if (control_pipe[1] != child_control_fd) {
      posix_spawn_file_actions_addclose(&actions, control_pipe[1]);
    }
    if (result_pipe[0] != child_result_fd) {
      posix_spawn_file_actions_addclose(&actions, result_pipe[0]);
    }
    if (result_pipe[1] != child_result_fd) {
      posix_spawn_file_actions_addclose(&actions, result_pipe[1]);
    }

    const auto affinity = affinity_string(affinity_);
    std::vector<std::string> storage{
        executable.string(),
        "process-worker",
        "--role",
        std::string{role_name(role_)},
        "--threads",
        std::to_string(thread_count),
        "--seed",
        std::to_string(seed),
        "--control-fd",
        std::to_string(child_control_fd),
        "--result-fd",
        std::to_string(child_result_fd),
    };
    if (!affinity.empty()) {
      storage.push_back("--affinity");
      storage.push_back(affinity);
    }
    std::vector<char*> arguments;
    arguments.reserve(storage.size() + 1U);
    for (auto& value : storage) {
      arguments.push_back(value.data());
    }
    arguments.push_back(nullptr);

    const auto spawn_result =
        posix_spawn(&pid_, executable.c_str(), &actions, nullptr, arguments.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    close(control_pipe[0]);
    close(result_pipe[1]);
    if (spawn_result != 0) {
      close_pipes();
      throw std::runtime_error("启动子进程失败: " + std::string{std::strerror(spawn_result)});
    }

    if (!wait_readable(result_fd_, kReadyTimeout)) {
      close_pipes();
      throw std::runtime_error("等待子进程 READY 超时: " + std::string{role_name(role_)});
    }
    ReadyMessage ready;
    if (!read_exact(result_fd_, &ready, sizeof(ready)) || ready.magic != process_protocol::kMagic ||
        ready.role != role_ || ready.pid != static_cast<std::uint64_t>(pid_)) {
      close_pipes();
      throw std::runtime_error("子进程 READY 协议无效: " + std::string{role_name(role_)});
    }
  }

  void send_frame(std::uint64_t frame_index) {
    const Request request{
        .magic = process_protocol::kMagic,
        .command = Command::RunFrame,
        .frame_index = frame_index,
    };
    if (!write_message(control_fd_, request)) {
      throw std::runtime_error("向子进程发送任务失败: " + std::string{role_name(role_)});
    }
  }

  [[nodiscard]] auto wait_reply() -> Reply {
    if (!wait_readable(result_fd_, kFrameTimeout)) {
      throw std::runtime_error("子进程帧任务超时: " + std::string{role_name(role_)});
    }
    Reply reply;
    if (!read_exact(result_fd_, &reply, sizeof(reply)) || reply.magic != process_protocol::kMagic) {
      throw std::runtime_error("子进程结果协议无效: " + std::string{role_name(role_)});
    }
    return reply;
  }

  void terminate() noexcept {
    if (pid_ <= 0) {
      close_pipes();
      return;
    }
    const Request stop{.magic = process_protocol::kMagic, .command = Command::Stop};
    (void)write_message(control_fd_, stop);
    close_pipes();

    const auto deadline = std::chrono::steady_clock::now() + kShutdownTimeout;
    while (std::chrono::steady_clock::now() < deadline) {
      int status = 0;
      const auto result = waitpid(pid_, &status, WNOHANG);
      if (result == pid_ || (result < 0 && errno == ECHILD)) {
        pid_ = -1;
        return;
      }
      if (result < 0 && errno != EINTR) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    (void)kill(pid_, SIGTERM);
    const auto terminate_deadline = std::chrono::steady_clock::now() + std::chrono::seconds{1};
    while (std::chrono::steady_clock::now() < terminate_deadline) {
      int status = 0;
      const auto result = waitpid(pid_, &status, WNOHANG);
      if (result == pid_ || (result < 0 && errno == ECHILD)) {
        pid_ = -1;
        return;
      }
      if (result < 0 && errno != EINTR) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    (void)kill(pid_, SIGKILL);
    int status = 0;
    (void)waitpid(pid_, &status, 0);
    pid_ = -1;
  }

  [[nodiscard]] auto role() const -> Role {
    return role_;
  }

private:
  void close_pipes() noexcept {
    if (control_fd_ >= 0) {
      close(control_fd_);
      control_fd_ = -1;
    }
    if (result_fd_ >= 0) {
      close(result_fd_);
      result_fd_ = -1;
    }
  }

  Role role_{};
  std::vector<unsigned> affinity_;
  pid_t pid_{-1};
  int control_fd_{-1};
  int result_fd_{-1};
};

[[nodiscard]] auto current_executable() -> std::filesystem::path {
  std::error_code error;
  const auto path = std::filesystem::read_symlink("/proc/self/exe", error);
  if (error || path.empty()) {
    throw std::runtime_error("无法解析当前 GameBench 可执行文件路径");
  }
  return path;
}

} // namespace

struct ProcessMixWorkload::Impl {
  explicit Impl(std::filesystem::path executable)
      : executable_(executable.empty() ? current_executable() : std::move(executable)) {
    struct sigaction ignore{};
    ignore.sa_handler = SIG_IGN;
    sigemptyset(&ignore.sa_mask);
    if (sigaction(SIGPIPE, nullptr, &previous_sigpipe_) == 0 &&
        sigaction(SIGPIPE, &ignore, nullptr) == 0) {
      sigpipe_changed_ = true;
    }
  }

  ~Impl() {
    shutdown();
    if (sigpipe_changed_) {
      (void)sigaction(SIGPIPE, &previous_sigpipe_, nullptr);
    }
  }

  void ensure(const WorkloadConfig& config);
  [[nodiscard]] auto schedule(const WorkloadConfig& config) -> WorkloadOutput;
  void shutdown() noexcept;

  std::filesystem::path executable_;
  std::string prepared_key_;
  unsigned main_thread_count_{};
  double launch_ms_{};
  std::uint64_t frame_index_{};
  std::vector<std::unique_ptr<ChildProcess>> children_;
  struct sigaction previous_sigpipe_{};
  bool sigpipe_changed_{};
};

void ProcessMixWorkload::Impl::ensure(const WorkloadConfig& config) {
  if (config.thread_count < 5) {
    throw std::invalid_argument(
        "game-process-mix 至少需要 5 个执行参与者：主进程 1 个 + 辅助进程 4 个");
  }
  std::string key = std::to_string(config.seed) + ':' + std::to_string(config.thread_count) + ':';
  for (const auto cpu : config.cpu_affinity) {
    key += std::to_string(cpu) + ',';
  }
  if (key == prepared_key_ && !children_.empty()) {
    return;
  }
  shutdown();
  prepared_key_ = key;
  frame_index_ = 0;
  main_thread_count_ = config.thread_count - 4U;

  std::vector<unsigned> main_affinity;
  std::vector<unsigned> scene_affinity;
  std::vector<unsigned> physics_affinity;
  std::vector<unsigned> ai_affinity;
  std::vector<unsigned> audio_affinity;
  if (!config.cpu_affinity.empty()) {
    main_affinity.assign(config.cpu_affinity.begin(),
                         config.cpu_affinity.begin() + main_thread_count_);
    scene_affinity.push_back(config.cpu_affinity[main_thread_count_]);
    physics_affinity.push_back(config.cpu_affinity[main_thread_count_ + 1U]);
    ai_affinity.push_back(config.cpu_affinity[main_thread_count_ + 2U]);
    audio_affinity.push_back(config.cpu_affinity[main_thread_count_ + 3U]);
  }

  const auto launch_start = std::chrono::steady_clock::now();
  children_.push_back(std::make_unique<ChildProcess>(Role::MainGame, std::move(main_affinity)));
  children_.push_back(
      std::make_unique<ChildProcess>(Role::SceneAuxiliary, std::move(scene_affinity)));
  children_.push_back(
      std::make_unique<ChildProcess>(Role::PhysicsAuxiliary, std::move(physics_affinity)));
  children_.push_back(std::make_unique<ChildProcess>(Role::AiAuxiliary, std::move(ai_affinity)));
  children_.push_back(
      std::make_unique<ChildProcess>(Role::AudioAuxiliary, std::move(audio_affinity)));
  try {
    children_[0]->spawn(executable_, main_thread_count_, config.seed);
    children_[1]->spawn(executable_, 1, config.seed ^ 0x9e3779b97f4a7c15ULL);
    children_[2]->spawn(executable_, 1, config.seed ^ 0xd1b54a32d192ed03ULL);
    children_[3]->spawn(executable_, 1, config.seed ^ 0x94d049bb133111ebULL);
    children_[4]->spawn(executable_, 1, config.seed ^ 0xbf58476d1ce4e5b9ULL);
  } catch (...) {
    shutdown();
    throw;
  }
  launch_ms_ =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - launch_start)
          .count();
}

auto ProcessMixWorkload::Impl::schedule(const WorkloadConfig& config) -> WorkloadOutput {
  ensure(config);
  const auto frame_index = frame_index_;
  const auto start = std::chrono::steady_clock::now();
  for (const auto& child : children_) {
    child->send_frame(frame_index);
  }

  std::array<Reply, 5> replies{};
  for (std::size_t index = 0; index < children_.size(); ++index) {
    replies[index] = children_[index]->wait_reply();
  }
  const auto end = std::chrono::steady_clock::now();

  std::uint64_t checksum = mix64(config.seed ^ frame_index);
  std::uint64_t operations = 0;
  bool valid = true;
  std::string error;
  for (std::size_t index = 0; index < replies.size(); ++index) {
    const auto& reply = replies[index];
    checksum = mix64(checksum ^ reply.checksum);
    operations += reply.operations;
    if (!reply.valid) {
      valid = false;
      if (error.empty()) {
        error = std::string{role_name(children_[index]->role())} + ": " + reply.error.data();
      }
    }
  }

  ProcessMetrics metrics{
      .coordinator_processes = 1,
      .game_processes = 5,
      .main_game_processes = 1,
      .auxiliary_processes = 4,
      .total_processes = 6,
      .main_thread_count = main_thread_count_,
      .auxiliary_thread_count = 4,
      .total_thread_count = config.thread_count,
      .launch_ms = launch_ms_,
      .main_process_cpu_ms = replies[0].process_cpu_time_ms,
      .auxiliary_cpu_ms = replies[1].process_cpu_time_ms + replies[2].process_cpu_time_ms +
                          replies[3].process_cpu_time_ms + replies[4].process_cpu_time_ms,
      .child_cpu_ms = std::accumulate(
          replies.begin(), replies.end(), 0.0,
          [](double total, const Reply& reply) { return total + reply.process_cpu_time_ms; }),
      .peak_rss_kb =
          std::max({replies[0].peak_rss_kb, replies[1].peak_rss_kb, replies[2].peak_rss_kb,
                    replies[3].peak_rss_kb, replies[4].peak_rss_kb}),
  };

  WorkloadOutput output;
  output.checksum = checksum;
  output.operations = operations;
  output.valid = valid && operations > 0 && checksum != 0;
  output.validation_message = output.valid ? std::string{} : std::move(error);
  output.measured_time_ms = std::chrono::duration<double, std::milli>(end - start).count();
  output.process = metrics;
  output.worker_count = main_thread_count_ - 1U + 4U;
  output.execution_mode = "main-game-plus-aux-processes";
  if (replies[0].simd_backend[0] != '\0') {
    output.simd_backend = replies[0].simd_backend.data();
  }
  ++frame_index_;
  return output;
}

void ProcessMixWorkload::Impl::shutdown() noexcept {
  for (const auto& child : children_) {
    child->terminate();
  }
  children_.clear();
  prepared_key_.clear();
  main_thread_count_ = 0;
  launch_ms_ = 0.0;
  frame_index_ = 0;
}

ProcessMixWorkload::ProcessMixWorkload(std::filesystem::path worker_executable)
    : impl_(std::make_unique<Impl>(std::move(worker_executable))) {}

ProcessMixWorkload::~ProcessMixWorkload() = default;

auto ProcessMixWorkload::id() const -> std::string_view {
  return "game-process-mix";
}

auto ProcessMixWorkload::version() const -> std::string_view {
  return "1";
}

auto ProcessMixWorkload::category() const -> std::string_view {
  return "engine-like";
}

auto ProcessMixWorkload::description() const -> std::string_view {
  return "一个主游戏进程与四个辅助进程并发的固定 Synthetic CPU 负载";
}

auto ProcessMixWorkload::execute(const WorkloadConfig& config) -> WorkloadOutput {
  return impl_->schedule(config);
}

} // namespace gamebench::benchmark
