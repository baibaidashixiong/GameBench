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
gamebench engine-info
gamebench list
gamebench run --workload integer --threads 1,2,4 --output result.json
gamebench run --workload scheduler --scheduler work-stealing --threads 1,2,4,8
gamebench run --workload ecs --layout soa --threads 1,2,4,8
gamebench run --workload memory --pattern pointer-chasing --working-set 16777216
gamebench run --workload scene --threads 1,2,4,8 --objects 50000
gamebench run --workload vulkan --mode no-submit --threads 1,2,4
gamebench run --workload vulkan --mode gpu-backed --threads 1,2,4
gamebench run --workload physics --threads 8 --objects 50000
gamebench run --workload animation --threads 1,2,4,8
gamebench run --workload ai --threads 1,2,4,8
gamebench run --workload audio --buffer-size 1024 --sample-rate 48000
gamebench run --workload scheduler --threads 2 --cpu-affinity 0,1 --hardware-counters
gamebench profile list
gamebench run --profile source2-like --threads 1,2,4,8,16
gamebench run --workload integer --format csv --output result.csv
gamebench compare result-a.json result-b.json
```

当前注册了 `integer`、`scheduler`、`ecs`、`memory`、`scene`、`vulkan`、`physics`、`animation`、`ai` 和 `audio` 工作负载。Vulkan 不可用时对应结果会标记为 `skipped`。Physics 当前使用原生轻量后端；当前环境未安装 Jolt Physics，因此 Jolt 后端尚未启用。后续工作负载只有在实现并通过校验后才会出现在 `gamebench list` 中。具体参数以 `gamebench --help` 的输出为准。

`engine-info` 显示真实引擎适配器状态。当前版本提供 Engine Adapter 契约、Godot 安装探测以及 Source 2 显式工具链探测，尚未启用引擎执行后端；UE5 与 O3DE 明确标记为未实现。Source 2 可使用 `SOURCE2_BIN=/path/to/tool` 或 `SOURCE2_ROOT=/path/to/root` 指定本地工具链。

`source2-like` Profile 不启动真实 Source 2，而是把 ECS、Scene、Physics、Animation、AI、Audio 和 Vulkan 录制组合成一条带依赖的 Synthetic 游戏帧。具体规模和限制见 `docs/source2-like-profile.md`。

`system-info` 会报告 hwloc 检测出的逻辑处理器、物理核心、SMT、NUMA、缓存、允许 CPU 列表以及可读取的频率和温度。可用 `--cpu-affinity 0,2`、`--physical-core 0` 或 `--numa-node 0` 选择绑定集合。无法可靠识别的 CCD 或 P/E Core 类型会明确显示 `Unknown`。硬件计数器受 `perf_event_open` 权限限制，不可用时结果保留为 `available: false`。

## 项目状态

开发顺序和验收状态见 [docs/development-roadmap.md](docs/development-roadmap.md)。测试方法、评分原则和架构约束分别记录在 `docs/` 中。

## 结果查看器

直接用浏览器打开 `tools/result_viewer/index.html`，载入一份或多份 GameBench JSON。查看器完全离线，支持线程扩展图、硬件与配置摘要、CSV 导出和 JSON 复制。

## 发布构建

```bash
./scripts/build-release.sh
```

脚本会运行 Release 测试、生成 `.build/package-root` 安装目录和 `.build/packages` 下的 TGZ。发布检查项见 `docs/release.md`。

## 许可证

本项目采用 MIT License，详见 [LICENSE](LICENSE)。

