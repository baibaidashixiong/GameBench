# Source 2-like Frame Profile

## 定位

`source2-like` 是 GameBench 的 Synthetic 单帧负载，不启动 Source 2、CS2 或其他真实游戏。它模拟现代游戏引擎一帧中的 CPU 任务组成，用于比较调度、拓扑和缓存配置，不代表任何真实游戏的绝对成绩。

Profile 与 `game-frame` 工作负载版本均为 `2`，结果 JSON 中的 `configuration.profile`、`benchmark_version` 和 `frame_profile` 共同定义测量语义。v2 扩大了固定工作集并加入 SIMD 粒子阶段，结果不得与 v1 基线直接比较。

## 默认规模

| 阶段 | 默认规模 |
| --- | --- |
| ECS Transform | 100000 实体 |
| Scene | 3 个独立场景，每个 30000 对象、8 个视图 |
| Physics | 8192 刚体、3 个串行子步 |
| Animation | 1024 个骨架、每骨架 64 骨骼 |
| AI/Navigation | 256 个 Agent 和 A* 查询 |
| SIMD Particle | 131072 个粒子、每帧 8 个积分步 |
| Audio | 64 个音源、1024 帧、48 kHz |
| Vulkan | 32 个录制任务、每任务 128 条命令、No-Submit |

粒子阶段使用 SSE2 intrinsic 更新位置、速度、重力和寿命。x86-64 上使用 `sse2` 后端；没有 SSE2 的构建使用标量回退。实际后端写入每份运行结果的 `simd_backend`，不同后端的分数不得直接混合比较。

## 帧任务图

1. ECS、Physics、Animation、AI、SIMD Particle、Scene View 和 Audio Source 任务并行进入 Job System。
2. 三个 Scene 集合使用不同固定种子，分别完成 8 个视图的 BVH 查询、Frustum Culling、LOD 选择和 Draw Call 准备。
3. SIMD 粒子按固定块拆分到多个 Job；每个 Job 只写自己的粒子块和 checksum 槽位。
4. Physics 子步之间使用依赖屏障，保证同一刚体不会跨子步并发写入。
5. Scene 汇总任务等待所有视图，Audio Mixing 等待所有音源。
6. Render Preparation 等待 ECS、Physics 和 Scene。
7. Vulkan Command Buffer 录制等待 Render Preparation。
8. 最终校验任务等待所有阶段，计算包含 SIMD 结果在内的确定性 checksum 和固定操作数。

每帧输出完整 wall time、主线程 CPU 时间、Worker 忙碌时间、关键路径、任务提交/完成数和 Work Stealing 次数。

`--threads 1` 使用 `main-inline`：不创建 Worker，所有 Job 由主线程执行。`--threads N` 使用 `main-plus-workers`：主线程和 `N-1` 个 Worker 共同执行。结果 JSON/CSV 记录 `execution_mode` 与 `worker_count`；线程扩展性以新的 1 线程主线程基线计算。

JSON 中的 `operations` 是当前线程配置全部采样帧的累计固定操作数，不同线程数应得到相同值；吞吐评分使用该累计值与中位帧时间计算，不会把模块 FPS 直接相加。

## 使用

```bash
gamebench profile list
gamebench run
gamebench run --profile source2-like --threads 1,2,4,8,16 --warmup 120 --repetitions 600
gamebench run --threads 1,2,4,8 --warmup 30 --repetitions 300
gamebench run --profile source2-like --threads 8 --cpu-affinity 0,1,2,3,4,5,6,7
```

正式比较必须使用 Release 构建、固定 Profile 版本、固定种子和相同 Affinity。Debug 数据只用于正确性调试。

直接执行 `gamebench run` 时使用 Profile v2 的固定规模、自动物理核心线程梯度、60 帧预热和 300 帧采样。每一帧执行相同阶段和规模，不注入人工帧时间波动；需要更长采样窗口时可继续提高 `--repetitions`。

## 限制

- 当前只有内置 Profile，尚未从 TOML 加载自定义 Profile。
- Profile 尚未使用真实 Source 2 trace 校准，因此只能称为 Source 2-like。
- SSE2 粒子任务扩大了指令和缓存压力，但仍不代表 Source 2 的真实粒子系统实现或 CS2 工作负载。
- Vulkan 不可用时该阶段跳过并写入 warning，其余 Synthetic 阶段继续运行。
- 不同 Profile 版本的结果不得直接比较。

