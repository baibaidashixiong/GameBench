#include "core/cli.hpp"

#include <algorithm>
#include <cstdlib>
#include <span>
#include <vector>

auto main(int argc, char** argv) -> int {
  std::vector<std::string_view> arguments;
  arguments.reserve(static_cast<std::size_t>(std::max(0, argc - 1)));
  for (int index = 1; index < argc; ++index) {
    arguments.emplace_back(argv[index]);
  }
  return gamebench::core::CliApplication{}.run(arguments);
}
