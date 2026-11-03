#pragma once

#include <cstddef>
#include <cstdint>

namespace gamebench::simulation {

[[nodiscard]] auto evaluate_skeleton(std::uint64_t seed, std::size_t bone_count, float time)
    -> std::uint64_t;

} // namespace gamebench::simulation
