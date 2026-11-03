# CPU 拓扑

GameBench 优先使用 hwloc，必要时使用 Linux sysfs 补充信息。检测内容包括逻辑处理器、物理核心、SMT、缓存层级、NUMA 节点和 CPU 特性。

Linux 逻辑 CPU 编号不保证对应物理核心、CCD 或 P/E Core 类型。只有平台提供可靠证据时才输出 CCD 或核心类型；否则输出 `Unknown`。

Affinity 配置以 Linux 逻辑 CPU 集合为准。用户可指定 CPU 列表、物理核心、NUMA 节点或策略生成的逻辑 CPU 集合。实际绑定失败必须作为配置错误报告，不能静默忽略。

