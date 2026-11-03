#pragma once

#include <span>
#include <vector>

namespace gamebench::platform {

[[nodiscard]] auto current_thread_affinity() -> std::vector<unsigned>;
void set_current_thread_affinity(std::span<const unsigned> cpus);

class ScopedThreadAffinity final {
public:
  explicit ScopedThreadAffinity(std::span<const unsigned> cpus);
  ~ScopedThreadAffinity();

  ScopedThreadAffinity(const ScopedThreadAffinity&) = delete;
  auto operator=(const ScopedThreadAffinity&) -> ScopedThreadAffinity& = delete;
  ScopedThreadAffinity(ScopedThreadAffinity&&) = delete;
  auto operator=(ScopedThreadAffinity&&) -> ScopedThreadAffinity& = delete;

private:
  std::vector<unsigned> original_;
  bool changed_{};
};

} // namespace gamebench::platform
