#pragma once

#include "benchmark/workload.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace gamebench::runtime {
class JobSystem;
}

namespace gamebench::benchmark {

class MemoryWorkload final : public Workload {
public:
  MemoryWorkload();
  ~MemoryWorkload() override;

  [[nodiscard]] auto id() const -> std::string_view override;
  [[nodiscard]] auto version() const -> std::string_view override;
  [[nodiscard]] auto category() const -> std::string_view override;
  [[nodiscard]] auto description() const -> std::string_view override;
  [[nodiscard]] auto execute(const WorkloadConfig& config) -> WorkloadOutput override;

private:
  void ensure_system(const WorkloadConfig& config);
  void ensure_storage(const WorkloadConfig& config);

  unsigned prepared_threads_{};
  core::SchedulerKind prepared_scheduler_{core::SchedulerKind::WorkStealing};
  std::size_t prepared_bytes_{};
  std::uint64_t prepared_seed_{};
  std::unique_ptr<runtime::JobSystem> system_;
  std::vector<std::uint64_t> payload_;
  std::vector<std::uint32_t> links_;
};

} // namespace gamebench::benchmark
