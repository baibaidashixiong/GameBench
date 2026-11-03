#include "platform/cpu_affinity.hpp"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

#include <pthread.h>
#include <sched.h>

namespace gamebench::platform {

auto current_thread_affinity() -> std::vector<unsigned> {
  cpu_set_t set;
  CPU_ZERO(&set);
  if (pthread_getaffinity_np(pthread_self(), sizeof(set), &set) != 0) {
    throw std::runtime_error("无法读取当前线程 CPU Affinity");
  }
  std::vector<unsigned> cpus;
  for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
    if (CPU_ISSET(cpu, &set) != 0) {
      cpus.push_back(static_cast<unsigned>(cpu));
    }
  }
  return cpus;
}

void set_current_thread_affinity(std::span<const unsigned> cpus) {
  if (cpus.empty()) {
    throw std::invalid_argument("CPU Affinity 不能为空");
  }
  cpu_set_t set;
  CPU_ZERO(&set);
  for (const auto cpu : cpus) {
    if (cpu >= CPU_SETSIZE) {
      throw std::invalid_argument("CPU 编号超出当前平台支持范围");
    }
    CPU_SET(cpu, &set);
  }
  const auto result = pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
  if (result != 0) {
    throw std::runtime_error("设置 CPU Affinity 失败: " + std::string{std::strerror(result)});
  }
}

ScopedThreadAffinity::ScopedThreadAffinity(std::span<const unsigned> cpus) {
  if (cpus.empty()) {
    return;
  }
  original_ = current_thread_affinity();
  set_current_thread_affinity(cpus);
  changed_ = true;
}

ScopedThreadAffinity::~ScopedThreadAffinity() {
  if (!changed_) {
    return;
  }
  try {
    set_current_thread_affinity(original_);
  } catch (...) {
  }
}

} // namespace gamebench::platform
