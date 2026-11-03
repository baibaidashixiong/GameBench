#include "profiler/hardware_counters.hpp"

#include <cstdint>
#include <unistd.h>

#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>

namespace gamebench::profiler {
namespace {

[[nodiscard]] auto perf_event_open(perf_event_attr* attribute, pid_t pid, int cpu, int group_fd,
                                   unsigned long flags) -> int {
  return static_cast<int>(syscall(SYS_perf_event_open, attribute, pid, cpu, group_fd, flags));
}

[[nodiscard]] auto open_counter(std::uint32_t type, std::uint64_t config) -> int {
  perf_event_attr attribute{};
  attribute.type = type;
  attribute.size = sizeof(attribute);
  attribute.config = config;
  attribute.disabled = 1;
  attribute.exclude_kernel = 1;
  attribute.exclude_hv = 1;
  return perf_event_open(&attribute, 0, -1, -1, PERF_FLAG_FD_CLOEXEC);
}

} // namespace

struct HardwareCounterSession::Impl {
  HardwareCounterResult result;
  int cycles_fd{-1};
  int instructions_fd{-1};
  int branches_fd{-1};
  int cache_fd{-1};
  int context_switches_fd{-1};
  bool stopped{};

  ~Impl() {
    for (const auto fd : {cycles_fd, instructions_fd, branches_fd, cache_fd, context_switches_fd}) {
      if (fd >= 0) {
        close(fd);
      }
    }
  }
};

HardwareCounterSession::HardwareCounterSession(bool enabled) : impl_(new Impl{}) {
  impl_->result.requested = enabled;
  if (!enabled) {
    return;
  }
  impl_->cycles_fd = open_counter(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES);
  impl_->instructions_fd = open_counter(PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);
  impl_->branches_fd = open_counter(PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES);
  impl_->cache_fd = open_counter(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES);
  impl_->context_switches_fd = open_counter(PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CONTEXT_SWITCHES);

  int enabled_count = 0;
  for (const auto fd : {impl_->cycles_fd, impl_->instructions_fd, impl_->branches_fd,
                        impl_->cache_fd, impl_->context_switches_fd}) {
    if (fd >= 0 && ioctl(fd, PERF_EVENT_IOC_ENABLE, 0) == 0) {
      ++enabled_count;
    }
  }
  impl_->result.available = enabled_count > 0;
  if (enabled_count == 0) {
    impl_->result.message = "perf_event_open 不可用：权限不足或内核配置限制";
  } else if (enabled_count < 5) {
    impl_->result.message = "部分硬件计数器不可用";
  }
}

HardwareCounterSession::~HardwareCounterSession() {
  (void)stop();
  delete impl_;
}

auto HardwareCounterSession::stop() -> HardwareCounterResult {
  if (impl_ == nullptr || impl_->stopped) {
    return impl_ == nullptr ? HardwareCounterResult{} : impl_->result;
  }
  impl_->stopped = true;
  auto read_counter = [](int fd) -> std::uint64_t {
    if (fd < 0) {
      return 0;
    }
    (void)ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
    std::uint64_t value = 0;
    const auto bytes = read(fd, &value, sizeof(value));
    return bytes == static_cast<ssize_t>(sizeof(value)) ? value : 0;
  };
  impl_->result.cycles = read_counter(impl_->cycles_fd);
  impl_->result.instructions = read_counter(impl_->instructions_fd);
  impl_->result.branch_misses = read_counter(impl_->branches_fd);
  impl_->result.cache_misses = read_counter(impl_->cache_fd);
  impl_->result.context_switches = read_counter(impl_->context_switches_fd);
  if (impl_->result.cycles != 0) {
    impl_->result.ipc =
        static_cast<double>(impl_->result.instructions) / static_cast<double>(impl_->result.cycles);
  }
  return impl_->result;
}

} // namespace gamebench::profiler
