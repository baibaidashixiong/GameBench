# 多进程游戏负载

## 定位

默认 `game-process-mix` 使用 5 个游戏子进程模拟常见的游戏运行拓扑；加上父协调进程后，操作系统层面共有 6 个进程：

| 角色 | 数量 | 固定负载 |
| --- | ---: | --- |
| 父协调进程 | 1 | 启动、同步、收集和校验，不执行游戏工作负载 |
| 主游戏进程 | 1 | Source 2-like `game-frame` CPU 阶段（当前不初始化 Vulkan） |
| Scene 辅助进程 | 1 | BVH 场景查询与 Draw Call 准备 |
| Physics 辅助进程 | 1 | 刚体、Broad-phase 与碰撞检测 |
| AI 辅助进程 | 1 | 行为树与 Navigation 查询 |
| Audio 辅助进程 | 1 | FIR、重采样、Reverb 与 Stereo Mixing |

这不是真实游戏进程拓扑的等价模型。它提供一个固定、可复现的多进程竞争负载，用于测试主游戏进程与辅助进程并发时的 CPU 调度和缓存压力。

## 线程分配

`--threads=N` 表示五个子进程合计的执行参与者数量，最低为 5。四个辅助进程各固定使用一个执行参与者，主游戏进程使用 `N-4` 个。例如：

- `--threads 5`：主进程 1，辅助进程各 1。
- `--threads 8`：主进程 4，辅助进程各 1。
- 默认根据物理核心生成从 5 开始的有效梯度。

显式 Affinity 按逻辑 CPU 顺序分配：主进程先获得其线程集合，之后 Scene、Physics、AI、Audio 各获得一个 CPU。集合数量不得少于 `--threads`。

## 进程协议

父进程使用 `posix_spawn` 启动当前 GameBench 可执行文件的内部 `process-worker` 命令。每个子进程拥有独立控制 Pipe 和结果 Pipe，并在内存中交换固定格式消息。

1. 子进程完成角色初始化后发送 READY。
2. 父进程向全部子进程发送同一帧任务。
3. 子进程执行固定负载、计算 checksum 和 CPU/RSS 数据后回传。
4. 父进程等待全部结果并执行联合校验。
5. 基准结束时发送 Stop；超时使用 SIGTERM，必要时清理并 reap。

结果记录 launch time、父协调进程 CPU、主游戏子进程 CPU、四个辅助进程 CPU、总子进程 CPU 和峰值单子进程 RSS。

默认预热阶段会先完成子进程启动，因此 `launch_ms` 不混入正式帧样本；显式设置 `--warmup 0` 时，首次启动开销会进入采样。

## 评分边界

多进程结果使用独立 `execution_domain: multi-process`，不能直接与单进程 FPS 相加。扩展性参考值为最小有效拓扑 5 个执行参与者，并写入 `thread_scaling_reference`。

当前不宣称该进程数量和辅助负载权重来自 CS2、Source 2 或其他真实游戏的 trace。

当前 `perf_event_open` 会话仍只覆盖父协调线程，尚未为每个子进程建立硬件计数器会话。子进程结果暂以 `CLOCK_PROCESS_CPUTIME_ID` 和 `getrusage` 为准。

第一版默认进程组合不初始化 Vulkan 设备，以隔离进程调度和 CPU 负载；需要测试 Vulkan CPU 录制时使用单进程 `game-frame` 工作负载。
