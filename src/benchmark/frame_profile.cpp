#include "benchmark/frame_profile.hpp"

namespace gamebench::benchmark {

auto make_source2_like_profile() -> FrameProfile {
  return {.id = "source2-like",
          .version = "2",
          .description = "Source 2 风格的单帧 Synthetic CPU 负载，不代表真实 CS2 成绩",
          .ecs_entities = 100000,
          .scene_sets = 3,
          .scene_objects = 30000,
          .scene_views = 8,
          .physics_bodies = 8192,
          .physics_substeps = 3,
          .animation_skeletons = 1024,
          .bones_per_skeleton = 64,
          .ai_agents = 256,
          .simd_particles = 131072,
          .simd_steps = 8,
          .audio_sources = 64,
          .audio_buffer_size = 1024,
          .sample_rate = 48000,
          .vulkan_tasks = 32,
          .vulkan_commands_per_task = 128};
}

auto available_frame_profiles() -> std::vector<FrameProfileDescriptor> {
  const auto profile = make_source2_like_profile();
  return {{.id = profile.id, .version = profile.version, .description = profile.description}};
}

auto find_frame_profile(std::string_view id) -> std::optional<FrameProfile> {
  if (id == "source2-like") {
    return make_source2_like_profile();
  }
  return std::nullopt;
}

} // namespace gamebench::benchmark
