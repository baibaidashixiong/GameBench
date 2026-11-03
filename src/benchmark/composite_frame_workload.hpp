#pragma once

#include "benchmark/workload.hpp"

#include <memory>

namespace gamebench::benchmark {

class CompositeFrameWorkload final : public Workload {
public:
  CompositeFrameWorkload();
  ~CompositeFrameWorkload() override;

  [[nodiscard]] auto id() const -> std::string_view override;
  [[nodiscard]] auto version() const -> std::string_view override;
  [[nodiscard]] auto category() const -> std::string_view override;
  [[nodiscard]] auto description() const -> std::string_view override;
  [[nodiscard]] auto execute(const WorkloadConfig& config) -> WorkloadOutput override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace gamebench::benchmark
