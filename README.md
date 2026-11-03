# GameBench

GameBench 是面向 Linux 的开源游戏 CPU 性能测试平台。它使用可复现的合成工作负载模拟现代游戏引擎中的任务调度、ECS、场景处理、动画、AI、音频 DSP 和 Vulkan CPU 端录制，同时为真实引擎适配器保留独立接口。

> 当前版本处于早期开发阶段。文档中标记为“计划中”的能力尚未交付，不应作为正式成绩引用。

## 环境要求

- Linux x86-64 或其他受支持架构
- 支持 C++23 的 GCC 或 Clang
- CMake 3.25 或更高版本
- Ninja
- GoogleTest（仅测试需要）
- hwloc（推荐）
- Vulkan Loader 与驱动（可选，缺失时 Vulkan 工作负载将跳过）

Fedora 上的依赖安装示例：

```bash
sudo dnf install cmake ninja-build gcc-c++ clang gtest-devel hwloc-devel vulkan-loader-devel vulkan-tools
```

## 构建与测试

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
./.build/release/gamebench --help
```

性能数据只能从 Release 构建采集。Debug 数据仅用于正确性调试。

## 命令

```bash
gamebench system-info
gamebench list
gamebench run --workload integer --threads 1,2,4 --output result.json
gamebench run --workload scheduler --scheduler work-stealing --threads 1,2,4,8
gamebench run --workload ecs --layout soa --threads 1,2,4,8
gamebench run --workload memory --pattern pointer-chasing --working-set 16777216
gamebench run --workload scene --threads 1,2,4,8 --objects 50000
gamebench run --workload integer --format csv --output result.csv
gamebench compare result-a.json result-b.json
```

当前注册了 `integer`、`scheduler`、`ecs`、`memory` 和 `scene` 工作负载。后续工作负载只有在实现并通过校验后才会出现在 `gamebench list` 中。具体参数以 `gamebench --help` 的输出为准。

## 项目状态

开发顺序和验收状态见 [docs/development-roadmap.md](docs/development-roadmap.md)。测试方法、评分原则和架构约束分别记录在 `docs/` 中。

## 许可证

本项目采用 MIT License，详见 [LICENSE](LICENSE)。

