#include "benchmark/registry.hpp"

#include "benchmark/ecs_workload.hpp"
#include "benchmark/integer_workload.hpp"
#include "benchmark/memory_workload.hpp"
#include "benchmark/scene_workload.hpp"
#include "benchmark/scheduler_workload.hpp"

#include <stdexcept>
#include <utility>

namespace gamebench::benchmark {

void WorkloadRegistry::add(WorkloadDescriptor descriptor, Factory factory) {
  if (descriptor.id.empty() || !factory) {
    throw std::invalid_argument("工作负载描述或工厂无效");
  }
  if (contains(descriptor.id)) {
    throw std::invalid_argument("重复的工作负载 ID: " + descriptor.id);
  }
  entries_.push_back(Entry{std::move(descriptor), std::move(factory)});
}

auto WorkloadRegistry::create(std::string_view id) const -> std::unique_ptr<Workload> {
  for (const auto& entry : entries_) {
    if (entry.descriptor.id == id) {
      return entry.factory();
    }
  }
  return nullptr;
}

auto WorkloadRegistry::descriptors() const -> std::vector<WorkloadDescriptor> {
  std::vector<WorkloadDescriptor> result;
  result.reserve(entries_.size());
  for (const auto& entry : entries_) {
    result.push_back(entry.descriptor);
  }
  return result;
}

auto WorkloadRegistry::contains(std::string_view id) const -> bool {
  for (const auto& entry : entries_) {
    if (entry.descriptor.id == id) {
      return true;
    }
  }
  return false;
}

auto WorkloadRegistry::create_default() -> WorkloadRegistry {
  WorkloadRegistry registry;
  registry.add({"integer", "1", "synthetic", "确定性整数混合负载"},
               [] { return std::make_unique<IntegerWorkload>(); });
  registry.add({"scheduler", "1", "synthetic", "三种调度模式的任务图负载"},
               [] { return std::make_unique<SchedulerWorkload>(); });
  registry.add({"ecs", "1", "synthetic", "AoS、SoA 与 AoSoA 实体组件负载"},
               [] { return std::make_unique<EcsWorkload>(); });
  registry.add({"memory", "1", "synthetic", "顺序、随机与指针追逐内存负载"},
               [] { return std::make_unique<MemoryWorkload>(); });
  registry.add({"scene", "1", "synthetic", "BVH 场景查询、剔除与 Draw Call 准备"},
               [] { return std::make_unique<SceneWorkload>(); });
  return registry;
}

} // namespace gamebench::benchmark
