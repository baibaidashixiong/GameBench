# CPU 拓扑

GameBench 优先使用 hwloc，必要时使用 Linux sysfs 补充信息。检测内容包括逻辑处理器、物理核心、SMT、缓存层级、NUMA 节点和 CPU 特性。

Linux 逻辑 CPU 编号不保证对应物理核心、CCD 或 P/E Core 类型。只有平台提供可靠证据时才输出 CCD 或核心类型；否则输出 `Unknown`。

Affinity 配置以 Linux 逻辑 CPU 集合为准。用户可指定 CPU 列表、物理核心、NUMA 节点或策略生成的逻辑 CPU 集合。实际绑定失败必须作为配置错误报告，不能静默忽略。

## 当前实现

- 优先使用 hwloc 检测逻辑处理器、物理核心、SMT、NUMA 和 L1-L5 缓存对象。
- 使用 `pthread_getaffinity_np` 与 `pthread_setaffinity_np` 读取和设置当前线程 Affinity，基准结束后恢复原集合。
- `--cpu-affinity 0,2,4,6` 中第一个 CPU 绑定主线程，其余 CPU 依次绑定 Worker；集合数量不得少于线程数。
- `--physical-core` 与 `--numa-node` 通过 hwloc cpuset 映射到逻辑 CPU；没有 hwloc 时会明确报错，而不是猜测映射。
- 无 hwloc 的构建使用 sysfs `topology`、`node` 与 `cache` 信息作为基础降级。
- CCD、P/E Core 类型没有可迁移的统一 Linux 接口，因此当前固定报告 `Unknown`。
- 频率、温度、Governor 仅作为环境诊断，不参与评分乘数。

