# 更新日志

本项目遵循 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/) 的结构，并计划采用语义化版本。

## [Unreleased]

### 新增

- 初始化 CMake、Ninja、CTest 与 Git 工程结构。
- 建立中文架构、测试方法、评分与 CPU 拓扑文档。
- 实现类型安全的运行配置、稳定计时统计、工作负载注册与 Benchmark Runner。
- 实现 JSON/CSV 结果导出、结果比较和 `run/list/system-info/compare` CLI。
- 添加确定性整数验证工作负载及 Linux 基础环境信息采集。
- 实现基于 `std::jthread` 的 Job System、原子依赖、异常传播和安全关闭。
- 支持 Global Queue、Static Partitioning、Work Stealing 三种调度模式。
- 添加调度工作负载、任务计数与窃取次数统计，并通过 ASan、UBSan、TSan 测试。
- 实现 AoS、SoA、AoSoA 三种 ECS 组件布局及实体创建、销毁和并行更新。
- 实现 Arena、Pool、顺序、随机、跨步与 Pointer Chasing 内存工作负载。
- 增加 ECS 布局和内存访问模式 CLI 配置与结果元数据。
- 实现场景 AABB、透视 Frustum、BVH、空间查询、可见性过滤、LOD 和 Draw Call 准备。
- 增加 BVH 与暴力遍历对照验证，并将场景和调度任务图固定为与线程数无关的总工作量。
- 实现可选 Vulkan 设备初始化、物理设备与驱动信息、每任务独立 Command Pool。
- 支持多线程 No-Submit Command Buffer 录制和 Fence 管理的 GPU-backed Offscreen 清屏。
- Vulkan 不可用时输出 Skipped；GPU-backed 计分时间排除 fence 等待并保留设备诊断信息。
- 实现原生刚体积分、Sweep-and-Prune Broad-phase、碰撞检测和空间查询工作负载。
- 实现动画采样、姿态混合、四元数骨骼传播和 Forward Kinematics 工作负载。
- 实现 Agent 决策树、A* Navigation 查询和路径生成工作负载。
- 实现多音源 FIR、上采样、线性重采样、Reverb 与 Stereo Mixing 离线 Audio DSP 工作负载。
- 增加 Audio Buffer 与采样率配置；当前环境缺少 Jolt Physics，原生 Physics 后端保持可用。
- 使用 hwloc 检测 CPU、SMT、NUMA 和缓存层级，并通过 sysfs 读取频率、温度与 Governor 信息。
- 实现可恢复的 CPU Affinity 配置，主线程与 Worker 可绑定到指定逻辑 CPU 集合。
- 实现可选 `perf_event_open` cycles、instructions、IPC、branch/cache miss 和 context switch 采集。
- 增加关键路径、主线程 CPU 时间、Worker 忙碌时间和主线程迁移统计。
- 增加独立 Engine Adapter 接口与 Godot 安装探测，明确 UE5/O3DE 未实现状态。
- 增加单工作负载吞吐与线程扩展评分，不跨操作单位生成虚假综合分。
- 增加离线 HTML 结果查看器、CSV 导出和多结果比较视图。
- 增加安装规则、CPack TGZ、Release 脚本和 GitHub Actions 构建矩阵。
- 增加 Source 2 Engine Adapter 探测，支持 `SOURCE2_BIN` 与 `SOURCE2_ROOT`，并明确标记为仅检测到、不可执行。
- 增加 `source2-like` 版本化 Frame Profile 与 `game-frame` 单帧合成工作负载。
- 使用依赖任务图组合 ECS、Scene、Physics、Animation、AI、Audio 与 Vulkan No-Submit 录制。
- 修复连续帧中 Scheduler 和 Game Frame 的统计基线，避免提交数与 Worker 忙碌时间跨帧累计。
- 简化默认命令：直接执行 `gamebench run` 会运行固定规模的 Source 2-like Profile，并自动生成物理核心线程梯度。
- 撤回 Source 2-like 负载波动模型，恢复所有帧使用固定 Profile 规模。
- 将 Source 2-like Profile 与 `game-frame` 升级到 v2，扩大 ECS、Physics、Animation、AI、Audio 和 Vulkan 固定规模。
- 增加三个独立 Scene 集合及 131072 粒子、每帧 8 步的 SSE2 粒子阶段，并在结果中记录 SIMD 后端。
- 增加 SIMD 粒子确定性、种子和时间步测试，并在无 SSE2 架构上保留标量回退。
- 将无参数默认运行调整为 60 帧预热和 300 帧采样，提高固定负载统计的稳定时间。
- 统一 `threads` 为包含主线程的总执行参与者；1 线程改为主线程闭环执行，不再创建 Worker。
- Job System 支持零 Worker 主线程消费模式，并覆盖全部 Job 型工作负载。
- 增加实际执行模型与 Worker 数量结果字段，并将主线程 Affinity 固定到指定集合的首个 CPU；CPU 时间改为按每帧样本计算一致的中位数。

