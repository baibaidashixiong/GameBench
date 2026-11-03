# Source 2-like Frame Profile

## 定位

`source2-like` 是 GameBench 的 Synthetic 单帧负载，不启动 Source 2、CS2 或其他真实游戏。它模拟现代游戏引擎一帧中的 CPU 任务组成，用于比较调度、拓扑和缓存配置，不代表任何真实游戏的绝对成绩。

Profile 版本为 `2`，结果 JSON 中的 `configuration.profile` 与工作负载版本共同定义测量语义。

## 默认规模

| 阶段 | 默认规模 |
| --- | --- |
| ECS Transform | 50000 实体 |
| Scene | 30000 对象、8 个视图 |
| Physics | 4096 刚体、2 个并行子步 |
| Animation | 512 个骨架、每骨架 64 骨骼 |
| AI/Navigation | 128 个 Agent 和 A* 查询 |
| Audio | 32 个音源、512 帧、48 kHz |
| Vulkan | 16 个录制任务、每任务 64 条命令、No-Submit |

## 帧任务图

1. ECS、Physics、Animation、AI、Scene View 和 Audio Source 任务并行进入 Job System。
2. Physics 子步之间使用依赖屏障，保证同一刚体不会跨子步并发写入。
3. Scene 汇总任务等待所有视图，Audio Mixing 等待所有音源。
4. Render Preparation 等待 ECS、Physics 和 Scene。
5. Vulkan Command Buffer 录制等待 Render Preparation。
6. 最终校验任务等待所有阶段，计算确定性 checksum 和操作数。

每帧输出完整 wall time、主线程 CPU 时间、Worker 忙碌时间、关键路径、任务提交/完成数和 Work Stealing 次数。

## 负载波动

Profile v2 使用固定种子驱动的确定性波动模型。每帧总负载在 45% 到 90% 之间变化，并有 8% 概率进入 100% 满载突发。ECS、Physics、Animation、AI、Scene View、Audio Source 和 Vulkan Command 数量会在此基础上进行独立小范围扰动。结果 JSON 记录实际 `mean_percent`、`minimum_percent` 和 `maximum_percent`。

## 使用

```bash
gamebench profile list
gamebench run
gamebench run --profile source2-like --threads 1,2,4,8,16 --warmup 120 --repetitions 600
gamebench run --profile source2-like --threads 8 --cpu-affinity 0,1,2,3,4,5,6,7
```

正式比较必须使用 Release 构建、固定 Profile 版本、固定种子和相同 Affinity。Debug 数据只用于正确性调试。

## 限制

- 当前只有内置 Profile，尚未从 TOML 加载自定义 Profile。
- Profile 尚未使用真实 Source 2 trace 校准，因此只能称为 Source 2-like。
- 波动模型用于覆盖调度和尾延迟场景，不等价于特定地图、模式或战斗事件。
- Vulkan 不可用时该阶段跳过并写入 warning，其余 Synthetic 阶段继续运行。
- 不同 Profile 版本的结果不得直接比较。

