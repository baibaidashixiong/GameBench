#include "simulation/animation.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <vector>

namespace gamebench::simulation {
namespace {

struct Rotation {
  float x{};
  float y{};
  float z{};
  float w{1.0F};
};

struct LocalPose {
  float translation[3]{};
  Rotation rotation;
  float scale[3]{1.0F, 1.0F, 1.0F};
};

struct GlobalPose {
  float translation[3]{};
  Rotation rotation;
};

[[nodiscard]] constexpr auto mix64(std::uint64_t value) -> std::uint64_t {
  value ^= value >> 30U;
  value *= 0xbf58476d1ce4e5b9ULL;
  value ^= value >> 27U;
  value *= 0x94d049bb133111ebULL;
  value ^= value >> 31U;
  return value;
}

[[nodiscard]] auto axis_angle(float x, float y, float z, float angle) -> Rotation {
  const auto half = angle * 0.5F;
  const auto sine = std::sin(half);
  return {x * sine, y * sine, z * sine, std::cos(half)};
}

[[nodiscard]] auto multiply(const Rotation& left, const Rotation& right) -> Rotation {
  return {left.w * right.x + left.x * right.w + left.y * right.z - left.z * right.y,
          left.w * right.y - left.x * right.z + left.y * right.w + left.z * right.x,
          left.w * right.z + left.x * right.y - left.y * right.x + left.z * right.w,
          left.w * right.w - left.x * right.x - left.y * right.y - left.z * right.z};
}

void rotate(const Rotation& rotation, float vector[3]) {
  const auto tx = 2.0F * (rotation.y * vector[2] - rotation.z * vector[1]);
  const auto ty = 2.0F * (rotation.z * vector[0] - rotation.x * vector[2]);
  const auto tz = 2.0F * (rotation.x * vector[1] - rotation.y * vector[0]);
  const auto x = vector[0] + rotation.w * tx + rotation.y * tz - rotation.z * ty;
  const auto y = vector[1] + rotation.w * ty + rotation.z * tx - rotation.x * tz;
  const auto z = vector[2] + rotation.w * tz + rotation.x * ty - rotation.y * tx;
  vector[0] = x;
  vector[1] = y;
  vector[2] = z;
}

[[nodiscard]] auto sample_pose(std::uint64_t seed, std::size_t bone, float time, float speed)
    -> LocalPose {
  const auto state = mix64(seed + bone * 0x9e3779b97f4a7c15ULL);
  const auto phase = static_cast<float>(state & 0xffffU) / 65535.0F * 6.2831853F;
  LocalPose pose;
  pose.translation[0] = std::sin(time * speed + phase) * 0.05F;
  pose.translation[1] = std::cos(time * speed * 0.75F + phase) * 0.03F;
  pose.translation[2] = 0.15F + std::sin(time * speed * 0.5F + phase) * 0.02F;
  pose.rotation = axis_angle(0.0F, 0.0F, 1.0F, std::sin(time * speed + phase) * 0.2F);
  pose.scale[0] = 0.95F + std::sin(phase + time) * 0.05F;
  pose.scale[1] = pose.scale[0];
  pose.scale[2] = pose.scale[0];
  return pose;
}

[[nodiscard]] auto blend(const LocalPose& first, const LocalPose& second, float weight)
    -> LocalPose {
  LocalPose result = first;
  for (std::size_t axis = 0; axis < 3; ++axis) {
    result.translation[axis] =
        first.translation[axis] * (1.0F - weight) + second.translation[axis] * weight;
    result.scale[axis] = first.scale[axis] * (1.0F - weight) + second.scale[axis] * weight;
  }
  result.rotation.x = first.rotation.x * (1.0F - weight) + second.rotation.x * weight;
  result.rotation.y = first.rotation.y * (1.0F - weight) + second.rotation.y * weight;
  result.rotation.z = first.rotation.z * (1.0F - weight) + second.rotation.z * weight;
  result.rotation.w = first.rotation.w * (1.0F - weight) + second.rotation.w * weight;
  const auto length =
      std::sqrt(result.rotation.x * result.rotation.x + result.rotation.y * result.rotation.y +
                result.rotation.z * result.rotation.z + result.rotation.w * result.rotation.w);
  if (length > 0.0F) {
    result.rotation.x /= length;
    result.rotation.y /= length;
    result.rotation.z /= length;
    result.rotation.w /= length;
  }
  return result;
}

} // namespace

auto evaluate_skeleton(std::uint64_t seed, std::size_t bone_count, float time) -> std::uint64_t {
  if (bone_count == 0) {
    return 0;
  }
  std::vector<GlobalPose> globals(bone_count);
  std::uint64_t checksum = mix64(seed ^ bone_count);
  for (std::size_t bone = 0; bone < bone_count; ++bone) {
    const auto first = sample_pose(seed, bone, time, 1.0F + static_cast<float>(bone % 5U) * 0.1F);
    const auto second = sample_pose(seed ^ 0xa5a5a5a5a5a5a5a5ULL, bone, time, 0.75F);
    const auto weight = static_cast<float>((bone + 1U) % 7U) / 8.0F;
    auto local = blend(first, second, weight);
    for (std::size_t axis = 0; axis < 3; ++axis) {
      local.translation[axis] *= local.scale[axis];
    }

    GlobalPose global = {
        .translation = {local.translation[0], local.translation[1], local.translation[2]},
        .rotation = local.rotation};
    if (bone > 0) {
      const auto parent = (bone - 1U) / 3U;
      float translated[3]{local.translation[0], local.translation[1], local.translation[2]};
      rotate(globals[parent].rotation, translated);
      global.translation[0] = globals[parent].translation[0] + translated[0];
      global.translation[1] = globals[parent].translation[1] + translated[1];
      global.translation[2] = globals[parent].translation[2] + translated[2];
      global.rotation = multiply(globals[parent].rotation, local.rotation);
    }
    globals[bone] = global;
    checksum = mix64(checksum ^ std::bit_cast<std::uint32_t>(global.translation[0]));
    checksum = mix64(checksum ^ std::bit_cast<std::uint32_t>(global.translation[1]));
    checksum = mix64(checksum ^ std::bit_cast<std::uint32_t>(global.translation[2]));
    checksum = mix64(checksum ^ std::bit_cast<std::uint32_t>(global.rotation.w));
  }
  return checksum;
}

} // namespace gamebench::simulation
