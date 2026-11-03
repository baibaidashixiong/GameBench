# GameBench 开发路线图

## 实施原则

- 按里程碑顺序交付，每个里程碑必须通过 Debug 和 Release 构建及测试后再提交。
- 只标记实际实现并验证过的能力；计划、占位接口和真实工作负载严格区分。
- 所有基准测试使用固定种子与正确性验证，默认记录环境和原始测量数据。
- Synthetic 与 Engine 后端独立评分；无法可靠识别的 CPU 拓扑信息标记为 Unknown。
- 性能结果只使用 Release 构建采集，Debug、ASan、UBSan 和 TSan 数据仅用于验证。

## 里程碑状态

| 里程碑 | 内容 | 状态 |
| --- | --- | --- |
| 0 | 项目基础设施、构建、测试、文档 | 已完成 |
| 1 | Core Runtime、JSON、CLI、基础 Runner | 已完成 |
| 2 | Job System、依赖、三种调度模式 | 已完成 |
| 3 | ECS 与 Memory Workload | 计划中 |
| 4 | Scene Processing | 计划中 |
| 5 | Vulkan Backend | 计划中 |
| 6 | Physics、Animation、AI、Audio | 计划中 |
| 7 | CPU Topology、Affinity、Profiling | 计划中 |
| 8 | 引擎适配器与 Godot 集成 | 计划中 |
| 9 | 结果查看器与正式发布 | 计划中 |

## 里程碑 0：项目基础设施

交付内容：

- Git 仓库、MIT 许可证、贡献指南和更新日志。
- CMakePresets、Ninja、严格编译告警和 CTest。
- Debug、Release、ASan、UBSan、TSan 构建入口。
- 可执行 CLI 骨架、核心日志模块和自动化冒烟测试。
- 中文架构、测试方法和评分文档。

验收标准：

- `cmake --preset debug && cmake --build --preset debug && ctest --preset debug` 通过。
- Release 构建与测试通过，`gamebench --help` 和 `gamebench --version` 可执行。

## 里程碑 1：Core Runtime

- Logger、运行配置、时间统计和 Benchmark Runner。
- JSON、CSV 结果输出与输入校验。
- `run`、`list`、`system-info`、`compare` 等可执行 CLI。
- 简单单线程工作负载端到端验证。

## 里程碑 2：Job System

- Worker 线程管理、全局队列、本地队列与 Work Stealing。
- Static Partitioning 和 Global Queue 调度模式。
- 原子依赖计数、等待与完成跟踪、异常传播和安全关闭。
- 相同工作负载下对三种调度模式做正确性与性能验证。

## 里程碑 3：ECS 与 Memory

- AoS、SoA、AoSoA 组件布局。
- Entity 创建/销毁、顺序遍历、Transform 与 Velocity 更新。
- Arena、Pool、顺序/随机访问、Pointer Chasing 工作负载。
- 可配置工作集、确定性种子和结果校验。

## 里程碑 4：Scene Processing

- BVH、AABB 查询、Frustum Culling、可见性过滤、LOD 与 Draw Call 准备。
- 可配置场景规模、对象分布和线程数量。

## 里程碑 5：Vulkan Backend

- 无窗口 Vulkan 初始化、设备和队列选择、每工作线程独立 Command Pool。
- No-Submit 与多 In-flight Frame 的轻量 Offscreen 模式。
- 无 Vulkan 环境时安全跳过，不让 GPU 时间主导 CPU 评分。

## 里程碑 6：Physics、Animation、AI 与 Audio

- 原生轻量 Physics、Animation、AI/Navigation 与 Audio DSP 工作负载。
- Jolt Physics 作为可选后端；未安装时明确标记。
- 每个工作负载支持规模、线程数、种子和正确性校验。

## 里程碑 7：CPU Topology 与 Profiling

- hwloc/sysfs 硬件检测、Affinity 与 NUMA 映射。
- 不确定 CCD、P/E Core 类型时输出 Unknown。
- 可选 perf 硬件计数器、分位数和调度统计。

## 里程碑 8：引擎适配

- 稳定 Engine Adapter 接口。
- 优先实现可选 Godot 集成，其后评估 Unreal Engine 5 与 O3DE。
- 引擎后端结果与 Synthetic 结果分开评分。

## 里程碑 9：结果查看器与发布

- 结果比较、CSV 导出和可视化工具。
- 完整可复现实验手册、发布包和版本化评分说明。

