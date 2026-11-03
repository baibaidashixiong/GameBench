#include "benchmark/composite_frame_workload.hpp"

#include "audio/dsp.hpp"
#include "benchmark/frame_profile.hpp"
#include "runtime/job_system.hpp"
#include "simulation/animation.hpp"
#include "simulation/ecs.hpp"
#include "simulation/navigation.hpp"
#include "simulation/physics.hpp"
#include "simulation/scene.hpp"
#include "simulation/simd_particles.hpp"

#if defined(GAMEBENCH_HAS_VULKAN)
#include "renderer/vulkan_context.hpp"
#endif

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace gamebench::benchmark {
namespace {

[[nodiscard]] constexpr auto mix64(std::uint64_t value) -> std::uint64_t {
  value ^= value >> 30U;
  value *= 0xbf58476d1ce4e5b9ULL;
  value ^= value >> 27U;
  value *= 0x94d049bb133111ebULL;
  value ^= value >> 31U;
  return value;
}

[[nodiscard]] auto combine(std::uint64_t checksum, const std::vector<std::uint64_t>& values)
    -> std::uint64_t {
  for (const auto value : values) {
    checksum = mix64(checksum ^ value);
  }
  return checksum;
}

[[nodiscard]] auto busy_sum(const runtime::SchedulerStatistics& statistics) -> double {
  return std::accumulate(statistics.worker_busy_ms.begin(), statistics.worker_busy_ms.end(), 0.0);
}

[[nodiscard]] auto chunk_count(std::size_t total, unsigned thread_count) -> std::size_t {
  return std::min(total, std::max<std::size_t>(1, static_cast<std::size_t>(thread_count) * 4U));
}

} // namespace

struct CompositeFrameWorkload::Impl {
  std::string prepared_key;
  FrameProfile profile;
  std::optional<runtime::SchedulerStatistics> previous_cumulative;
  std::uint64_t frame_index{};
  std::unique_ptr<runtime::JobSystem> jobs;
  std::unique_ptr<simulation::EcsWorld> ecs;
  std::unique_ptr<simulation::PhysicsWorld> physics;
  std::vector<std::unique_ptr<simulation::Scene>> scenes;
  std::unique_ptr<simulation::SimdParticleSystem> simd_particles;
  std::unique_ptr<simulation::NavigationGrid> navigation;
#if defined(GAMEBENCH_HAS_VULKAN)
  std::unique_ptr<renderer::VulkanContext> vulkan;
#endif
  std::optional<VulkanDiagnostics> vulkan_diagnostics;
  std::string vulkan_warning;

  void ensure(const WorkloadConfig& config);
  [[nodiscard]] auto schedule(const WorkloadConfig& config) -> WorkloadOutput;
};

void CompositeFrameWorkload::Impl::ensure(const WorkloadConfig& config) {
  const auto selected = find_frame_profile(config.profile_id);
  if (!selected) {
    throw std::invalid_argument("未知 Frame Profile: " + config.profile_id);
  }
  if (selected->ecs_entities == 0 || selected->scene_sets == 0 || selected->scene_objects == 0 ||
      selected->scene_views == 0 || selected->physics_bodies == 0 ||
      selected->physics_substeps == 0 || selected->animation_skeletons == 0 ||
      selected->bones_per_skeleton == 0 || selected->ai_agents == 0 ||
      selected->simd_particles == 0 || selected->simd_steps == 0 || selected->audio_sources == 0 ||
      selected->audio_buffer_size == 0 || selected->sample_rate == 0 ||
      selected->vulkan_tasks == 0 || selected->vulkan_commands_per_task == 0) {
    throw std::invalid_argument("Frame Profile 包含无效的零规模参数");
  }
  std::string key = selected->id + ':' + selected->version + ':' +
                    std::to_string(config.thread_count) + ':' +
                    std::to_string(static_cast<int>(config.scheduler)) + ':' +
                    (config.enable_vulkan ? "vulkan-on:" : "vulkan-off:");
  for (const auto cpu : config.cpu_affinity) {
    key += std::to_string(cpu) + ',';
  }
  if (key == prepared_key) {
    return;
  }

  jobs.reset();
  prepared_key = key;
  profile = *selected;
  frame_index = 0;
  previous_cumulative.reset();
  ecs = std::make_unique<simulation::EcsWorld>(core::DataLayout::SoA, profile.ecs_entities);
  for (std::size_t index = 0; index < profile.ecs_entities; ++index) {
    (void)ecs->create_entity();
  }
  physics = std::make_unique<simulation::PhysicsWorld>(profile.physics_bodies, 0x5eed);
  scenes.clear();
  scenes.reserve(profile.scene_sets);
  for (unsigned scene_set = 0; scene_set < profile.scene_sets; ++scene_set) {
    scenes.push_back(std::make_unique<simulation::Scene>(
        profile.scene_objects,
        0x5eedULL + static_cast<std::uint64_t>(scene_set) * 0x9e3779b97f4a7c15ULL));
  }
  simd_particles = std::make_unique<simulation::SimdParticleSystem>(profile.simd_particles, 0x5eed);
  navigation = std::make_unique<simulation::NavigationGrid>(64, 64, 0x5eed);
  jobs = std::make_unique<runtime::JobSystem>(runtime::JobSystemConfig{
      .worker_count = job_worker_count(config),
      .main_thread_participates = config.thread_count > 0,
      .scheduler = config.scheduler,
      .cpu_affinity = worker_affinity(config),
  });

  vulkan_diagnostics.reset();
  vulkan_warning.clear();
#if defined(GAMEBENCH_HAS_VULKAN)
  vulkan.reset();
  if (config.enable_vulkan) {
    try {
      vulkan = std::make_unique<renderer::VulkanContext>(profile.vulkan_tasks);
      vulkan_diagnostics = VulkanDiagnostics{.device_name = vulkan->device_name(),
                                             .driver_name = vulkan->driver_name(),
                                             .api_version = vulkan->api_version()};
    } catch (const std::exception& exception) {
      vulkan_warning = std::string{"Vulkan 阶段跳过: "} + exception.what();
    }
  } else {
    vulkan_warning = "Vulkan 阶段已由配置禁用";
  }
#else
  vulkan_warning = "Vulkan 阶段跳过: 构建时未找到 Vulkan SDK";
#endif
}

auto CompositeFrameWorkload::Impl::schedule(const WorkloadConfig& config) -> WorkloadOutput {
  ensure(config);
  const auto seed = config.seed + frame_index * 0x9e3779b97f4a7c15ULL;
  std::vector<runtime::JobHandle> completion_dependencies;
  std::vector<std::uint64_t> ecs_checksums;
  std::vector<std::uint64_t> animation_checksums(profile.animation_skeletons, 0);
  std::vector<std::uint64_t> ai_checksums(profile.ai_agents, 0);
  std::vector<std::uint64_t> ai_path_lengths(profile.ai_agents, 0);
  const auto scene_view_count = static_cast<std::size_t>(profile.scene_sets) * profile.scene_views;
  std::vector<std::uint64_t> scene_checksums(scene_view_count, 0);
  std::vector<std::uint64_t> scene_visible(scene_view_count, 0);
  std::vector<std::uint64_t> simd_checksums;
  std::vector<std::vector<audio::StereoFrame>> audio_sources(profile.audio_sources);
  std::vector<audio::StereoFrame> mixed_audio;
  std::vector<std::uint64_t> vulkan_checksums;
  std::uint64_t render_checksum = 0;
  std::uint64_t final_checksum = 0;
  std::uint64_t operations = 0;
  bool phases_valid = true;

  const auto ecs_tasks = chunk_count(profile.ecs_entities, config.thread_count);
  const auto ecs_chunk = (profile.ecs_entities + ecs_tasks - 1U) / ecs_tasks;
  ecs_checksums.resize(ecs_tasks, 0);
  std::vector<runtime::JobHandle> ecs_handles;
  ecs_handles.reserve(ecs_tasks);
  for (std::size_t task = 0; task < ecs_tasks; ++task) {
    const auto begin = task * ecs_chunk;
    const auto end = std::min(profile.ecs_entities, begin + ecs_chunk);
    ecs_handles.push_back(jobs->submit([&, task, begin, end] {
      ecs->update(begin, end, 1.0F / 60.0F);
      ecs_checksums[task] = ecs->checksum(begin, end);
    }));
  }
  completion_dependencies.insert(completion_dependencies.end(), ecs_handles.begin(),
                                 ecs_handles.end());

  std::vector<runtime::JobHandle> physics_previous;
  for (unsigned substep = 0; substep < profile.physics_substeps; ++substep) {
    const auto tasks = chunk_count(profile.physics_bodies, config.thread_count);
    const auto chunk = (profile.physics_bodies + tasks - 1U) / tasks;
    std::vector<runtime::JobHandle> current;
    current.reserve(tasks);
    for (std::size_t task = 0; task < tasks; ++task) {
      const auto begin = task * chunk;
      const auto end = std::min(profile.physics_bodies, begin + chunk);
      current.push_back(jobs->submit(
          [&, begin, end] { physics->integrate(begin, end, 1.0F / 120.0F); }, physics_previous));
    }
    physics_previous = std::move(current);
  }
  completion_dependencies.insert(completion_dependencies.end(), physics_previous.begin(),
                                 physics_previous.end());

  const auto animation_tasks = chunk_count(profile.animation_skeletons, config.thread_count);
  const auto animation_chunk =
      (profile.animation_skeletons + animation_tasks - 1U) / animation_tasks;
  std::vector<runtime::JobHandle> animation_handles;
  for (std::size_t task = 0; task < animation_tasks; ++task) {
    const auto begin = task * animation_chunk;
    const auto end = std::min(profile.animation_skeletons, begin + animation_chunk);
    animation_handles.push_back(jobs->submit([&, begin, end] {
      for (auto skeleton = begin; skeleton < end; ++skeleton) {
        animation_checksums[skeleton] =
            simulation::evaluate_skeleton(seed + skeleton, profile.bones_per_skeleton,
                                          static_cast<float>(frame_index + skeleton % 31U) / 60.0F);
      }
    }));
  }
  completion_dependencies.insert(completion_dependencies.end(), animation_handles.begin(),
                                 animation_handles.end());

  const auto ai_tasks = chunk_count(profile.ai_agents, config.thread_count);
  const auto ai_chunk = (profile.ai_agents + ai_tasks - 1U) / ai_tasks;
  std::vector<runtime::JobHandle> ai_handles;
  for (std::size_t task = 0; task < ai_tasks; ++task) {
    const auto begin = task * ai_chunk;
    const auto end = std::min(profile.ai_agents, begin + ai_chunk);
    ai_handles.push_back(jobs->submit([&, begin, end] {
      for (auto agent = begin; agent < end; ++agent) {
        const auto state = mix64(seed + agent);
        const auto goal =
            simulation::GridCell{static_cast<std::uint16_t>(1U + state % 63U),
                                 static_cast<std::uint16_t>(1U + (state >> 16U) % 63U)};
        const auto path = navigation->find_path({0, 0}, goal);
        const auto action =
            simulation::evaluate_behavior({.position_x = 0.0F,
                                           .position_y = 0.0F,
                                           .health = static_cast<float>(state & 0xffU),
                                           .distance_to_target = static_cast<float>(path.size())});
        ai_path_lengths[agent] = path.size();
        ai_checksums[agent] = mix64(state ^ path.size() ^ static_cast<std::uint8_t>(action));
      }
    }));
  }
  completion_dependencies.insert(completion_dependencies.end(), ai_handles.begin(),
                                 ai_handles.end());

  const auto simd_tasks = chunk_count(simd_particles->block_count(), config.thread_count);
  const auto simd_chunk = (simd_particles->block_count() + simd_tasks - 1U) / simd_tasks;
  simd_checksums.resize(simd_tasks, 0);
  std::vector<runtime::JobHandle> simd_handles;
  simd_handles.reserve(simd_tasks);
  for (std::size_t task = 0; task < simd_tasks; ++task) {
    const auto begin = task * simd_chunk;
    const auto end = std::min(simd_particles->block_count(), begin + simd_chunk);
    simd_handles.push_back(jobs->submit([&, task, begin, end] {
      simd_particles->update(begin, end, profile.simd_steps, 1.0F / 120.0F);
      simd_checksums[task] = simd_particles->checksum(begin, end);
    }));
  }
  completion_dependencies.insert(completion_dependencies.end(), simd_handles.begin(),
                                 simd_handles.end());

  std::vector<runtime::JobHandle> scene_handles;
  scene_handles.reserve(scene_view_count);
  for (unsigned scene_set = 0; scene_set < profile.scene_sets; ++scene_set) {
    for (unsigned view = 0; view < profile.scene_views; ++view) {
      const auto scene_index = static_cast<std::size_t>(scene_set) * profile.scene_views + view;
      scene_handles.push_back(jobs->submit([&, scene_set, view, scene_index] {
        auto frustum = simulation::Frustum::perspective(1.0F, 100.0F, 0.7002075F);
        frustum = frustum.translated({static_cast<float>(view % 8U) * 4.0F - 16.0F,
                                      static_cast<float>(view / 8U) * 4.0F - 8.0F, 0.0F});
        const auto query = scenes[scene_set]->process_view(frustum, seed + scene_index);
        scene_checksums[scene_index] = query.checksum;
        scene_visible[scene_index] = query.visible_objects;
      }));
    }
  }
  completion_dependencies.insert(completion_dependencies.end(), scene_handles.begin(),
                                 scene_handles.end());

  const auto audio_tasks = chunk_count(profile.audio_sources, config.thread_count);
  const auto audio_chunk = (profile.audio_sources + audio_tasks - 1U) / audio_tasks;
  std::vector<runtime::JobHandle> audio_source_handles;
  for (std::size_t task = 0; task < audio_tasks; ++task) {
    const auto begin = task * audio_chunk;
    const auto end = std::min(profile.audio_sources, begin + audio_chunk);
    audio_source_handles.push_back(jobs->submit([&, begin, end] {
      for (auto source = begin; source < end; ++source) {
        audio_sources[source] =
            audio::render_source(seed, source, profile.audio_buffer_size, profile.sample_rate);
      }
    }));
  }
  auto audio_mix = jobs->submit(
      [&] { mixed_audio = audio::mix_sources(audio_sources, profile.audio_buffer_size); },
      audio_source_handles);
  completion_dependencies.push_back(audio_mix);

  auto render_dependencies = scene_handles;
  render_dependencies.insert(render_dependencies.end(), ecs_handles.begin(), ecs_handles.end());
  render_dependencies.insert(render_dependencies.end(), physics_previous.begin(),
                             physics_previous.end());
  auto render_prep = jobs->submit(
      [&] {
        render_checksum = mix64(
            seed ^ std::accumulate(scene_visible.begin(), scene_visible.end(), std::uint64_t{0}));
        for (const auto checksum : scene_checksums) {
          render_checksum = mix64(render_checksum ^ checksum);
        }
        phases_valid = phases_valid && std::ranges::any_of(scene_visible, [](std::uint64_t count) {
                         return count > 0;
                       });
      },
      render_dependencies);
  completion_dependencies.push_back(render_prep);

#if defined(GAMEBENCH_HAS_VULKAN)
  if (vulkan) {
    vulkan_checksums.resize(profile.vulkan_tasks, 0);
    std::vector<runtime::JobHandle> vulkan_handles;
    for (unsigned task = 0; task < profile.vulkan_tasks; ++task) {
      const std::array dependencies{render_prep};
      vulkan_handles.push_back(jobs->submit(
          [&, task] {
            vulkan->record_no_submit(task, profile.vulkan_commands_per_task,
                                     static_cast<std::uint32_t>(seed + task));
            vulkan_checksums[task] =
                mix64(seed ^ static_cast<std::uint64_t>(task) ^ profile.vulkan_commands_per_task);
          },
          dependencies));
    }
    completion_dependencies.insert(completion_dependencies.end(), vulkan_handles.begin(),
                                   vulkan_handles.end());
  }
#endif

  auto final_job = jobs->submit(
      [&] {
        const auto collisions = physics->collect_collisions();
        phases_valid =
            phases_valid && collisions.contact_pairs <= collisions.broad_phase_pairs &&
            ecs->alive_count() == profile.ecs_entities && scenes.size() == profile.scene_sets &&
            simd_particles->block_count() == (profile.simd_particles + 3U) / 4U &&
            std::ranges::all_of(simd_checksums, [](std::uint64_t value) { return value != 0; }) &&
            !mixed_audio.empty();
        final_checksum = mix64(seed);
        final_checksum = combine(final_checksum, ecs_checksums);
        final_checksum = combine(final_checksum, animation_checksums);
        final_checksum = combine(final_checksum, ai_checksums);
        final_checksum = combine(final_checksum, simd_checksums);
        final_checksum = mix64(final_checksum ^ render_checksum);
        final_checksum = mix64(final_checksum ^ collisions.broad_phase_pairs ^
                               (collisions.contact_pairs << 16U));
        for (const auto& frame : mixed_audio) {
          final_checksum = mix64(final_checksum ^ std::bit_cast<std::uint32_t>(frame.left));
          final_checksum = mix64(final_checksum ^ std::bit_cast<std::uint32_t>(frame.right));
        }
        final_checksum = combine(final_checksum, vulkan_checksums);
        operations =
            profile.ecs_entities + profile.physics_bodies * profile.physics_substeps +
            profile.animation_skeletons * profile.bones_per_skeleton + profile.ai_agents +
            std::accumulate(ai_path_lengths.begin(), ai_path_lengths.end(), std::uint64_t{0}) +
            profile.scene_objects * profile.scene_sets * profile.scene_views +
            profile.simd_particles * profile.simd_steps +
            profile.audio_sources * profile.audio_buffer_size * 8U +
            (vulkan_diagnostics ? profile.vulkan_tasks * profile.vulkan_commands_per_task : 0U);
      },
      completion_dependencies);
  jobs->wait(final_job);

  WorkloadOutput output;
  output.checksum = final_checksum;
  output.operations = operations;
  output.valid = phases_valid && final_checksum != 0;
  if (!output.valid) {
    output.validation_message = "Source 2 风格单帧工作负载校验失败";
  }
  if (!vulkan_warning.empty()) {
    output.warnings.push_back(vulkan_warning);
  }
  output.vulkan = vulkan_diagnostics;
  output.simd_backend = simulation::simd_backend_name();
  set_job_execution_metadata(output, config);
  const auto current = jobs->statistics();
  const runtime::SchedulerStatistics empty_statistics;
  const auto& previous = previous_cumulative.value_or(empty_statistics);
  output.scheduling = SchedulingMetrics{
      .submitted_jobs = current.submitted_jobs - previous.submitted_jobs,
      .completed_jobs = current.completed_jobs - previous.completed_jobs,
      .stolen_jobs = current.stolen_jobs - previous.stolen_jobs,
      .main_thread_jobs = current.main_thread_jobs - previous.main_thread_jobs,
      .exceptions = current.exceptions - previous.exceptions,
      .worker_busy_ms = busy_sum(current) - busy_sum(previous),
  };
  previous_cumulative = current;
  ++frame_index;
  return output;
}

CompositeFrameWorkload::CompositeFrameWorkload() : impl_(std::make_unique<Impl>()) {}
CompositeFrameWorkload::~CompositeFrameWorkload() = default;

auto CompositeFrameWorkload::id() const -> std::string_view {
  return "game-frame";
}
auto CompositeFrameWorkload::version() const -> std::string_view {
  return "2";
}
auto CompositeFrameWorkload::category() const -> std::string_view {
  return "engine-like";
}
auto CompositeFrameWorkload::description() const -> std::string_view {
  return "版本化 Profile 驱动的 Source 2 风格单帧 Synthetic CPU 负载";
}

auto CompositeFrameWorkload::execute(const WorkloadConfig& config) -> WorkloadOutput {
  return impl_->schedule(config);
}

} // namespace gamebench::benchmark
