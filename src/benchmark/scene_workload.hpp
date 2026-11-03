#pragma once

#include "benchmark/workload.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace gamebench::runtime {
class JobSystem;
}

namespace gamebench::simulation {
class Scene;
}

namespace gamebench::benchmark {

class SceneWorkload final : public Workload {
public:
  SceneWorkload();
  ~SceneWorkload() override;

  [[nodiscard]] auto id() const -> std::string_view override;
  [[nodiscard]] auto version() const -> std::string_view override;
  [[nodiscard]] auto category() const -> std::string_view override;
  [[nodiscard]] auto description() const -> std::string_view override;
  [[nodiscard]] auto execute(const WorkloadConfig& config) -> WorkloadOutput override;

private:
  void ensure_system(const WorkloadConfig& config);
  void ensure_scene(const WorkloadConfig& config);

  unsigned prepared_threads_{};
  core::SchedulerKind prepared_scheduler_{core::SchedulerKind::WorkStealing};
  std::size_t prepared_objects_{};
  std::uint64_t prepared_seed_{};
  bool scene_valid_{};
  std::unique_ptr<runtime::JobSystem> system_;
  std::unique_ptr<simulation::Scene> scene_;
};

} // namespace gamebench::benchmark
