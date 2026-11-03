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

## 可复现性

每个工作负载接收规范化配置、固定随机种子和显式线程数。运行结果记录硬件、环境、编译类型、Affinity 和版本信息。异常值不会被静默删除。

