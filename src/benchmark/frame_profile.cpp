#include "benchmark/frame_profile.hpp"

namespace gamebench::benchmark {

auto make_source2_like_profile() -> FrameProfile {
  return {.id = "source2-like",
          .version = "1",
          .description = "Source 2 风格的单帧 Synthetic CPU 负载，不代表真实 CS2 成绩",
          .ecs_entities = 50000,
          .scene_objects = 30000,
          .scene_views = 8,
          .physics_bodies = 4096,
          .physics_substeps = 2,
          .animation_skeletons = 512,
          .bones_per_skeleton = 64,
          .ai_agents = 128,
          .audio_sources = 32,
          .audio_buffer_size = 512,
          .sample_rate = 48000,
          .vulkan_tasks = 16,
          .vulkan_commands_per_task = 64};
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
