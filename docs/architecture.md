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

## 可复现性

每个工作负载接收规范化配置、固定随机种子和显式线程数。运行结果记录硬件、环境、编译类型、Affinity 和版本信息。异常值不会被静默删除。

