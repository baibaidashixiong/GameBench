#include "core/logger.hpp"
#include "core/version.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

void print_help() {
  std::cout << "GameBench " << gamebench::core::kVersion << "\n"
            << "面向 Linux 的游戏引擎 CPU 性能测试平台\n\n"
            << "用法:\n"
            << "  gamebench --help\n"
            << "  gamebench --version\n"
            << "  gamebench system-info\n\n"
            << "当前版本仅提供基础设施命令。Benchmark 运行命令将在后续里程碑启用。\n";
}

[[nodiscard]] auto is_one_of(std::string_view value,
                             const auto& candidates) -> bool {
  for (const auto candidate : candidates) {
    if (value == candidate) {
      return true;
    }
  }
  return false;
}

} // namespace

auto main(int argc, char** argv) -> int {
  using namespace std::string_view_literals;

  if (argc == 1) {
    print_help();
    return EXIT_SUCCESS;
  }

  const std::string_view command = argv[1];
  constexpr std::array help_flags{"--help"sv, "-h"sv, "help"sv};
  constexpr std::array version_flags{"--version"sv, "-V"sv};

  if (is_one_of(command, help_flags)) {
    print_help();
    return EXIT_SUCCESS;
  }

  if (is_one_of(command, version_flags)) {
    std::cout << gamebench::core::kProjectName << ' ' << gamebench::core::kVersion << '\n';
    return EXIT_SUCCESS;
  }

  if (command == "system-info"sv) {
    gamebench::core::Logger logger;
    logger.warning("详细 CPU 拓扑检测将在里程碑 7 完成");
    std::cout << "GameBench 版本: " << gamebench::core::kVersion << '\n';
    return EXIT_SUCCESS;
  }

  std::cerr << "未知命令: " << command << "\n\n";
  print_help();
  return EXIT_FAILURE;
}

