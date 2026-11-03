# 架构说明

## 分层

- `core`：应用入口、配置、Benchmark Runner、结果与日志。
- `runtime`：Job System、调度、内存、帧调度与 Frame Graph。
- `simulation`：ECS、Physics、Animation、AI、Navigation 与场景处理。
- `renderer`：Vulkan 设备管理、CPU 端准备和 Command Buffer 录制。
- `audio`：离线 DSP、混音和混响。
- `profiler`：计时、线程统计、帧统计与可选硬件计数器。
- `platform`：Linux CPU 拓扑、Affinity 和 NUMA。
- `benchmark`：工作负载注册、套件、后端适配与评分。

模块通过明确的数据结构交互。Benchmark 注册中心是工作负载扩展点，新增工作负载不修改 Runner 的调度核心。

## 依赖策略

核心运行时和主要合成工作负载不依赖大型第三方库。hwloc、Vulkan、Jolt Physics 和引擎集成均为可选能力，缺失时必须降级为明确报告或跳过。GoogleTest 只用于测试。

## 线程与所有权

RAII 管理线程、Vulkan 设备、内存池和结果文件。任务系统通过原子依赖计数和完成事件管理生命周期。关闭流程先停止接收任务，再排空或取消未执行任务，最后回收所有 Worker。

Job System 支持 Global Queue、Static Partitioning 和 Work Stealing 三种模式。每个任务拥有原子依赖计数，JobSystem 在任务完成前保留强引用；依赖满足后任务才进入调度队列。异常被捕获到任务状态中，由等待方显式重新抛出。主线程可在等待期间参与执行任务，Worker 可通过配置的 Linux 逻辑 CPU 列表设置 Affinity。

## ECS 与内存

ECS 工作负载实现 AoS、SoA 和 16 实体分块的 AoSoA 布局，共享相同的实体代际、创建、销毁和组件更新语义。Arena 提供可重置的线性分配，Pool 提供固定大小块的复用；内存工作负载支持顺序、随机、跨步和 Pointer Chasing 模式。所有结果校验值只由组件或访问数据决定，不混入存储布局等配置枚举。

## 场景处理

场景工作负载构造确定性的 AABB 对象集合和基于最长质心轴的 BVH。每个视图执行 AABB 空间查询、Frustum Culling、可见对象过滤、LOD 选择和 Draw Command 生成。自动化测试对 BVH 可见集合计数与暴力遍历对照；线程扩展性测试使用固定的 64 个视图分片，保证不同线程数执行相同总工作。

## Vulkan

Vulkan 后端是可选模块。构建时找到 Vulkan Loader 后启用，运行时不强制要求可用设备。No-Submit 模式并行录制 Command Buffer 但不提交；GPU-backed 模式采用三组 Fence 提交轻量 Offscreen 图像清屏。每个录制分片拥有独立 Command Pool，避免跨线程复用外部同步。GPU-backed 的样本时间记录 CPU 端录制与提交调用，fence 等待在采样结束后执行，设备、驱动和 API 版本作为诊断写入结果。

Mesa/RADV 等驱动可能在 LeakSanitizer 中报告驱动级分配，并在 ThreadSanitizer 中报告驱动内部同步。测试只对 Vulkan 驱动聚合测试关闭 LeakSanitizer，并在 TSan 构建中跳过 Vulkan 驱动调用；GameBench 自有线程与内存模块仍运行完整 Sanitizer 检查。

## 游戏模拟与音频

Physics 使用确定性的原生刚体、积分、Sweep-and-Prune Broad-phase、球体碰撞检测和空间查询。Jolt Physics 保留为可选后端，但只有实际完成适配并通过测试后才会出现在工作负载列表中；当前环境未安装 Jolt，因此没有伪造 Jolt 结果。

Animation 工作负载对两套程序化动画采样并进行姿态混合，在固定骨骼层级上执行四元数变换与 Forward Kinematics。AI 工作负载结合行为决策树和 A* Navigation 查询。Audio 工作负载在 CPU 上完成多通道合成、FIR、双倍采样输入、线性重采样、Reverb 和 Stereo Mixing，不访问音频设备。

## 平台与 Profiling

hwloc 提供 CPU 拓扑与缓存层次，sysfs 提供 Governor、频率和可选温度。Affinity 使用作用域绑定并在基准结束后恢复，Worker 使用独立逻辑 CPU 配置。`perf_event_open` 采集 cycles、instructions、IPC、branch miss、cache miss 和 context switch；权限不足时只记录不可用原因，不影响 Benchmark 完成。

结果记录 wall-clock 关键路径、主线程 CPU 时间、Job System Worker 忙碌时间和主线程迁移次数。无法可靠分离的 synchronization overhead 不会被估算或伪造，后续只有在增加明确测量点后才会输出。

## Source 2-like 单帧负载

`game-frame` 工作负载读取版本化 `FrameProfile`，为各阶段配置独立规模。Physics 子步通过依赖链建立屏障；Scene 汇总、Audio Mixing、Render Preparation 和 Vulkan 录制按任务依赖执行；最后以单一校验任务等待整个帧完成。该负载用于测试 CPU 调度与现代游戏帧工作组成，不代表真实 Source 2 或 CS2 成绩。

## 可复现性

每个工作负载接收规范化配置、固定随机种子和显式线程数。运行结果记录硬件、环境、编译类型、Affinity 和版本信息。异常值不会被静默删除。

