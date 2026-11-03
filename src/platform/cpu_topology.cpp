#include "platform/cpu_topology.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <sched.h>
#include <unistd.h>

#if defined(GAMEBENCH_HAS_HWLOC)
#include <hwloc.h>
#endif

namespace gamebench::platform {
namespace {

[[nodiscard]] auto current_allowed_cpus() -> std::vector<unsigned> {
  std::vector<unsigned> cpus;
  cpu_set_t set;
  CPU_ZERO(&set);
  if (sched_getaffinity(0, sizeof(set), &set) != 0) {
    return cpus;
  }
  for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
    if (CPU_ISSET(cpu, &set) != 0) {
      cpus.push_back(static_cast<unsigned>(cpu));
    }
  }
  return cpus;
}

#if !defined(GAMEBENCH_HAS_HWLOC)
[[nodiscard]] auto read_text_file(const std::filesystem::path& path) -> std::string {
  std::ifstream input(path);
  std::string value;
  std::getline(input, value);
  return value;
}

[[nodiscard]] auto fallback_physical_cores() -> unsigned {
  std::set<std::pair<std::string, std::string>> cores;
  const std::filesystem::path cpu_root{"/sys/devices/system/cpu"};
  std::error_code error;
  if (!std::filesystem::is_directory(cpu_root, error)) {
    return 0;
  }
  for (const auto& entry : std::filesystem::directory_iterator(cpu_root, error)) {
    const auto name = entry.path().filename().string();
    if (name.size() <= 3 || name.rfind("cpu", 0) != 0 ||
        !std::isdigit(static_cast<unsigned char>(name[3])) ||
        name.find_first_not_of("0123456789", 3) != std::string::npos) {
      continue;
    }
    const auto topology = entry.path() / "topology";
    const auto package = read_text_file(topology / "physical_package_id");
    const auto core = read_text_file(topology / "core_id");
    if (!package.empty() && !core.empty()) {
      cores.emplace(package, core);
    }
  }
  return static_cast<unsigned>(cores.size());
}

[[nodiscard]] auto fallback_numa_nodes() -> unsigned {
  unsigned count = 0;
  std::error_code error;
  for (const auto& entry : std::filesystem::directory_iterator("/sys/devices/system/node", error)) {
    if (entry.path().filename().string().rfind("node", 0) == 0) {
      ++count;
    }
  }
  return count;
}

[[nodiscard]] auto parse_cache_size(const std::string& text) -> std::uint64_t {
  if (text.empty()) {
    return 0;
  }
  std::uint64_t value = 0;
  std::size_t index = 0;
  while (index < text.size() && std::isdigit(static_cast<unsigned char>(text[index]))) {
    value = value * 10U + static_cast<std::uint64_t>(text[index] - '0');
    ++index;
  }
  if (index < text.size()) {
    if (text[index] == 'K') {
      value *= 1024U;
    } else if (text[index] == 'M') {
      value *= 1024U * 1024U;
    }
  }
  return value;
}

[[nodiscard]] auto fallback_caches() -> std::vector<CpuCacheInfo> {
  std::vector<CpuCacheInfo> caches;
  for (unsigned index = 0; index < 16; ++index) {
    const auto base = std::filesystem::path{"/sys/devices/system/cpu/cpu0/cache"} /
                      ("index" + std::to_string(index));
    const auto level = read_text_file(base / "level");
    const auto type = read_text_file(base / "type");
    const auto size = read_text_file(base / "size");
    if (level.empty()) {
      continue;
    }
    caches.push_back({.level = static_cast<std::uint32_t>(std::stoul(level)),
                      .type = type.empty() ? "Unknown" : type,
                      .size_bytes = parse_cache_size(size),
                      .shared_by = 0});
  }
  return caches;
}
#endif

} // namespace

auto detect_cpu_topology() -> CpuTopology {
  CpuTopology result;
  result.logical_processors = std::thread::hardware_concurrency();
  result.physical_cores = result.logical_processors;
  result.allowed_cpus = current_allowed_cpus();

#if defined(GAMEBENCH_HAS_HWLOC)
  hwloc_topology_t topology = nullptr;
  if (hwloc_topology_init(&topology) == 0) {
    hwloc_topology_set_flags(topology, HWLOC_TOPOLOGY_FLAG_IS_THISSYSTEM);
    if (hwloc_topology_load(topology) == 0) {
      const auto logical = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_PU);
      const auto cores = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_CORE);
      const auto numa_nodes = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_NUMANODE);
      if (logical > 0) {
        result.logical_processors = static_cast<unsigned>(logical);
      }
      if (cores > 0) {
        result.physical_cores = static_cast<unsigned>(cores);
      }
      result.numa_nodes = numa_nodes > 0 ? static_cast<unsigned>(numa_nodes) : 1U;
      result.smt_enabled = result.logical_processors > result.physical_cores;

      constexpr hwloc_obj_type_t cache_types[] = {HWLOC_OBJ_L1CACHE, HWLOC_OBJ_L2CACHE,
                                                  HWLOC_OBJ_L3CACHE, HWLOC_OBJ_L4CACHE,
                                                  HWLOC_OBJ_L5CACHE};
      for (const auto cache_type : cache_types) {
        for (auto* object = hwloc_get_next_obj_by_type(topology, cache_type, nullptr);
             object != nullptr; object = hwloc_get_next_obj_by_type(topology, cache_type, object)) {
          if (object->attr == nullptr) {
            continue;
          }
          std::string type = "Unknown";
          if (object->attr->cache.type == HWLOC_OBJ_CACHE_DATA) {
            type = "Data";
          } else if (object->attr->cache.type == HWLOC_OBJ_CACHE_INSTRUCTION) {
            type = "Instruction";
          } else if (object->attr->cache.type == HWLOC_OBJ_CACHE_UNIFIED) {
            type = "Unified";
          }
          const auto shared_by =
              object->cpuset == nullptr
                  ? 0U
                  : static_cast<std::uint32_t>(hwloc_bitmap_weight(object->cpuset));
          result.caches.push_back({.level = object->attr->cache.depth,
                                   .type = std::move(type),
                                   .size_bytes = object->attr->cache.size,
                                   .shared_by = shared_by});
        }
      }
    }
    hwloc_topology_destroy(topology);
  }
#else
  const auto fallback_cores = fallback_physical_cores();
  if (fallback_cores > 0) {
    result.physical_cores = fallback_cores;
  }
  result.smt_enabled = result.logical_processors > result.physical_cores;
  const auto fallback_nodes = fallback_numa_nodes();
  result.numa_nodes = fallback_nodes > 0 ? fallback_nodes : 1;
  result.caches = fallback_caches();
#endif

  if (result.numa_nodes == 0) {
    result.numa_nodes = 1;
  }
  std::ranges::sort(result.caches, [](const CpuCacheInfo& left, const CpuCacheInfo& right) {
    if (left.level != right.level) {
      return left.level < right.level;
    }
    return left.type < right.type;
  });
  return result;
}

namespace {

#if defined(GAMEBENCH_HAS_HWLOC)
[[nodiscard]] auto cpuset_to_vector(hwloc_const_cpuset_t cpuset) -> std::vector<unsigned> {
  std::vector<unsigned> cpus;
  if (cpuset == nullptr) {
    return cpus;
  }
  const auto count = hwloc_bitmap_weight(cpuset);
  cpus.reserve(static_cast<std::size_t>(count));
  int cpu = -1;
  while ((cpu = hwloc_bitmap_next(cpuset, cpu)) >= 0) {
    cpus.push_back(static_cast<unsigned>(cpu));
  }
  return cpus;
}
#endif

} // namespace

auto logical_cpus_for_physical_core(unsigned core_index) -> std::vector<unsigned> {
#if defined(GAMEBENCH_HAS_HWLOC)
  hwloc_topology_t topology = nullptr;
  if (hwloc_topology_init(&topology) != 0) {
    throw std::runtime_error("无法初始化 hwloc 拓扑");
  }
  if (hwloc_topology_load(topology) != 0) {
    hwloc_topology_destroy(topology);
    throw std::runtime_error("无法加载 hwloc 拓扑");
  }
  auto* core = hwloc_get_obj_by_type(topology, HWLOC_OBJ_CORE, core_index);
  auto cpus = cpuset_to_vector(core == nullptr ? nullptr : core->cpuset);
  hwloc_topology_destroy(topology);
  if (cpus.empty()) {
    throw std::out_of_range("物理核心索引不存在或不包含逻辑 CPU");
  }
  return cpus;
#else
  (void)core_index;
  throw std::runtime_error("构建时未启用 hwloc，无法按物理核心选择 CPU");
#endif
}

auto logical_cpus_for_numa_node(unsigned node_index) -> std::vector<unsigned> {
#if defined(GAMEBENCH_HAS_HWLOC)
  hwloc_topology_t topology = nullptr;
  if (hwloc_topology_init(&topology) != 0) {
    throw std::runtime_error("无法初始化 hwloc 拓扑");
  }
  if (hwloc_topology_load(topology) != 0) {
    hwloc_topology_destroy(topology);
    throw std::runtime_error("无法加载 hwloc 拓扑");
  }
  auto* node = hwloc_get_obj_by_type(topology, HWLOC_OBJ_NUMANODE, node_index);
  auto cpus = cpuset_to_vector(node == nullptr ? nullptr : node->cpuset);
  hwloc_topology_destroy(topology);
  if (cpus.empty()) {
    throw std::out_of_range("NUMA 节点索引不存在或不包含逻辑 CPU");
  }
  return cpus;
#else
  (void)node_index;
  throw std::runtime_error("构建时未启用 hwloc，无法按 NUMA 节点选择 CPU");
#endif
}

} // namespace gamebench::platform
