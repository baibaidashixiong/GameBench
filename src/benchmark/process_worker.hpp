#pragma once

#include <span>
#include <string_view>

namespace gamebench::benchmark {

[[nodiscard]] auto run_process_worker(std::span<const std::string_view> arguments) -> int;

} // namespace gamebench::benchmark
