#pragma once

#include "benchmark/workload.hpp"

namespace gamebench::benchmark {

class IntegerWorkload final : public Workload {
public:
  [[nodiscard]] auto id() const -> std::string_view override;
  [[nodiscard]] auto version() const -> std::string_view override;
  [[nodiscard]] auto category() const -> std::string_view override;
  [[nodiscard]] auto description() const -> std::string_view override;
  [[nodiscard]] auto execute(const WorkloadConfig& config) -> WorkloadOutput override;
};

} // namespace gamebench::benchmark

