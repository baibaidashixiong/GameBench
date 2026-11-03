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

