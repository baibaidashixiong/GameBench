#pragma once

#include "benchmark/workload.hpp"

#include <memory>

namespace gamebench::runtime {
class JobSystem;
}

namespace gamebench::benchmark {

class EcsWorkload final : public Workload {
public:
  EcsWorkload();
  ~EcsWorkload() override;

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
};

} // namespace gamebench::benchmark
