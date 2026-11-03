#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace gamebench::platform {

struct CpuCacheInfo {
  std::uint32_t level{};
  std::string type;
  std::uint64_t size_bytes{};
  std::uint32_t shared_by{};
};

struct CpuTopology {
  unsigned logical_processors{};
  unsigned physical_cores{};
  bool smt_enabled{};
  unsigned numa_nodes{};
  std::vector<unsigned> allowed_cpus;
  std::vector<CpuCacheInfo> caches;
  std::string ccd_type{"Unknown"};
  std::string core_type{"Unknown"};
};

[[nodiscard]] auto detect_cpu_topology() -> CpuTopology;
[[nodiscard]] auto logical_cpus_for_physical_core(unsigned core_index) -> std::vector<unsigned>;
[[nodiscard]] auto logical_cpus_for_numa_node(unsigned node_index) -> std::vector<unsigned>;

} // namespace gamebench::platform
