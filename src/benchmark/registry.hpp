#pragma once

#include "benchmark/workload.hpp"

#include <functional>
#include <memory>
#include <string_view>
#include <vector>

namespace gamebench::benchmark {

struct WorkloadDescriptor {
  std::string id;
  std::string version;
  std::string category;
  std::string description;
};

class WorkloadRegistry final {
public:
  using Factory = std::function<std::unique_ptr<Workload>()>;

  void add(WorkloadDescriptor descriptor, Factory factory);
  [[nodiscard]] auto create(std::string_view id) const -> std::unique_ptr<Workload>;
  [[nodiscard]] auto descriptors() const -> std::vector<WorkloadDescriptor>;
  [[nodiscard]] auto contains(std::string_view id) const -> bool;

  static auto create_default() -> WorkloadRegistry;

private:
  struct Entry {
    WorkloadDescriptor descriptor;
    Factory factory;
  };

  std::vector<Entry> entries_;
};

} // namespace gamebench::benchmark
