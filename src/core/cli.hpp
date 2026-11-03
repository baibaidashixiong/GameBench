#pragma once

#include "core/config.hpp"

#include <span>
#include <string_view>

namespace gamebench::core {

[[nodiscard]] auto parse_run_config(std::span<const std::string_view> arguments) -> RunConfig;

class CliApplication final {
public:
  [[nodiscard]] auto run(std::span<const std::string_view> arguments) const -> int;
};

} // namespace gamebench::core
