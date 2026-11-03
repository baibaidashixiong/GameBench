#include "benchmark/registry.hpp"

#include "benchmark/ai_workload.hpp"
#include "benchmark/animation_workload.hpp"
#include "benchmark/audio_workload.hpp"
#include "benchmark/composite_frame_workload.hpp"
#include "benchmark/ecs_workload.hpp"
#include "benchmark/integer_workload.hpp"
#include "benchmark/memory_workload.hpp"
#include "benchmark/physics_workload.hpp"
#include "benchmark/scene_workload.hpp"
#include "benchmark/scheduler_workload.hpp"
#include "benchmark/vulkan_workload.hpp"

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
  registry.add({"vulkan", "1", "synthetic", "Vulkan Command Buffer 录制与 Offscreen 清屏"},
               [] { return std::make_unique<VulkanWorkload>(); });
  registry.add({"physics", "1", "synthetic", "刚体积分、Broad-phase 与碰撞检测"},
               [] { return std::make_unique<PhysicsWorkload>(); });
  registry.add({"animation", "1", "synthetic", "动画采样、混合与骨骼传播"},
               [] { return std::make_unique<AnimationWorkload>(); });
  registry.add({"ai", "1", "synthetic", "Agent 决策树与 A* Navigation 查询"},
               [] { return std::make_unique<AiWorkload>(); });
  registry.add({"audio", "1", "synthetic", "FIR、重采样、Reverb 与 Stereo Mixing"},
               [] { return std::make_unique<AudioWorkload>(); });
  registry.add({"game-frame", "2", "engine-like", "版本化 Profile 驱动的单帧 Synthetic CPU 负载"},
               [] { return std::make_unique<CompositeFrameWorkload>(); });
  return registry;
}

} // namespace gamebench::benchmark
