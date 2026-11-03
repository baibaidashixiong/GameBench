#pragma once

#include "benchmark/workload.hpp"

#include <cstdint>
#include <memory>

namespace gamebench::runtime {
class JobSystem;
}

namespace gamebench::benchmark {

class SchedulerWorkload final : public Workload {
public:
  SchedulerWorkload();
  ~SchedulerWorkload() override;

  [[nodiscard]] auto id() const -> std::string_view override;
  [[nodiscard]] auto version() const -> std::string_view override;
  [[nodiscard]] auto category() const -> std::string_view override;
  [[nodiscard]] auto description() const -> std::string_view override;
  [[nodiscard]] auto execute(const WorkloadConfig& config) -> WorkloadOutput override;

private:
  void ensure_system(const WorkloadConfig& config);

  unsigned prepared_threads_{};
  core::SchedulerKind prepared_scheduler_{core::SchedulerKind::WorkStealing};
  std::unique_ptr<runtime::JobSystem> system_;
  SchedulingMetrics previous_cumulative_{};
  bool has_previous_statistics_{};
};

} // namespace gamebench::benchmark
