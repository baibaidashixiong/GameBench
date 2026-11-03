#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gamebench::benchmark {

struct FrameProfile {
  std::string id;
  std::string version;
  std::string description;
  std::size_t ecs_entities{};
  std::size_t scene_objects{};
  unsigned scene_views{};
  std::size_t physics_bodies{};
  unsigned physics_substeps{};
  std::size_t animation_skeletons{};
  std::size_t bones_per_skeleton{};
  std::size_t ai_agents{};
  std::size_t audio_sources{};
  std::uint32_t audio_buffer_size{};
  std::uint32_t sample_rate{};
  unsigned vulkan_tasks{};
  std::uint32_t vulkan_commands_per_task{};
};

struct FrameProfileDescriptor {
  std::string id;
  std::string version;
  std::string description;
};

[[nodiscard]] auto make_source2_like_profile() -> FrameProfile;
[[nodiscard]] auto available_frame_profiles() -> std::vector<FrameProfileDescriptor>;
[[nodiscard]] auto find_frame_profile(std::string_view id) -> std::optional<FrameProfile>;

} // namespace gamebench::benchmark
