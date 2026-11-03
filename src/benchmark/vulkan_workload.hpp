#pragma once

#include "benchmark/workload.hpp"

#include <memory>
#include <string>

namespace gamebench::renderer {
#if defined(GAMEBENCH_HAS_VULKAN)
class VulkanContext;
#else
class VulkanContext final {};
#endif
} // namespace gamebench::renderer

namespace gamebench::runtime {
class JobSystem;
}

namespace gamebench::benchmark {

class VulkanWorkload final : public Workload {
public:
  VulkanWorkload();
  ~VulkanWorkload() override;

  [[nodiscard]] auto id() const -> std::string_view override;
  [[nodiscard]] auto version() const -> std::string_view override;
  [[nodiscard]] auto category() const -> std::string_view override;
  [[nodiscard]] auto description() const -> std::string_view override;
  [[nodiscard]] auto execute(const WorkloadConfig& config) -> WorkloadOutput override;

private:
  void ensure_system(const WorkloadConfig& config);
  [[nodiscard]] auto ensure_context(const WorkloadConfig& config) -> bool;

  unsigned prepared_threads_{};
  core::SchedulerKind prepared_scheduler_{core::SchedulerKind::WorkStealing};
  std::string context_error_;
  std::unique_ptr<runtime::JobSystem> system_;
  std::unique_ptr<renderer::VulkanContext> context_;
};

} // namespace gamebench::benchmark
