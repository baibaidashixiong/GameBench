cmake_minimum_required(VERSION 3.25)

project(
  GameBench
  VERSION 0.1.0
  DESCRIPTION "面向 Linux 的游戏引擎 CPU 性能测试平台"
  LANGUAGES CXX
)

include(CTest)
include(GNUInstallDirs)

option(GAMEBENCH_BUILD_TESTS "构建 GameBench 测试" ON)
option(GAMEBENCH_ENABLE_VULKAN "启用 Vulkan 工作负载" ON)
option(GAMEBENCH_ENABLE_HWLOC "启用 hwloc CPU 拓扑检测" ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(gamebench_warnings INTERFACE)
target_compile_options(
  gamebench_warnings
  INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wshadow>
)

find_package(Threads REQUIRED)
if(GAMEBENCH_ENABLE_VULKAN)
  find_package(Vulkan QUIET)
endif()

add_library(gamebench_core STATIC
  src/benchmark/ecs_workload.cpp
  src/benchmark/integer_workload.cpp
  src/benchmark/memory_workload.cpp
  src/benchmark/registry.cpp
  src/benchmark/scheduler_workload.cpp
  src/benchmark/scene_workload.cpp
  src/benchmark/vulkan_workload.cpp
  src/core/cli.cpp
  src/core/config.cpp
  src/core/logger.cpp
  src/core/result_manager.cpp
  src/core/result_reader.cpp
  src/core/runner.cpp
  src/core/timing.cpp
  src/platform/system_info.cpp
  src/runtime/job_system.cpp
  src/runtime/memory_system.cpp
  src/simulation/ecs.cpp
  src/simulation/scene.cpp
)
target_include_directories(gamebench_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(gamebench_core PUBLIC Threads::Threads gamebench_warnings)

if(Vulkan_FOUND)
  target_sources(gamebench_core PRIVATE src/renderer/vulkan_context.cpp)
  target_compile_definitions(gamebench_core PUBLIC GAMEBENCH_HAS_VULKAN=1)
  target_link_libraries(gamebench_core PUBLIC Vulkan::Vulkan)
  message(STATUS "Vulkan 已启用: ${Vulkan_VERSION}")
else()
  message(STATUS "Vulkan 未找到，vulkan 工作负载将报告 Skipped")
endif()

add_executable(gamebench src/main.cpp)
target_link_libraries(gamebench PRIVATE gamebench_core)

if(GAMEBENCH_BUILD_TESTS)
  find_package(GTest REQUIRED)
  include(GoogleTest)

  add_executable(gamebench_tests
    tests/cli_test.cpp
    tests/ecs_test.cpp
    tests/memory_system_test.cpp
    tests/smoke_test.cpp
    tests/timing_test.cpp
    tests/runner_test.cpp
    tests/scene_test.cpp
    tests/job_system_test.cpp
    tests/workload_test.cpp
    tests/vulkan_test.cpp
  )
  target_link_libraries(gamebench_tests PRIVATE gamebench_core GTest::gtest_main)
  gtest_discover_tests(gamebench_tests TEST_FILTER "-VulkanTest.*")
  add_test(NAME VulkanTest COMMAND gamebench_tests --gtest_filter=VulkanTest.*)
  set_tests_properties(VulkanTest PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()

install(TARGETS gamebench RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

