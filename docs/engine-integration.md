# 游戏引擎接入

## 边界

真实引擎工作负载与 Synthetic Workload 使用独立后端、独立结果和独立评分域。Engine Adapter 只向上层暴露探测、配置和结果结构，不把引擎绝对运行时间与合成工作负载直接相加。

当前 `EngineAdapter` 接口定义：

- `EngineAdapterInfo`：引擎 ID、版本、可用状态和诊断信息。
- `EngineBenchmarkRequest`：固定种子、对象规模、线程数、预热帧、采样帧和项目路径。
- `EngineBenchmarkOutput`：原始帧时间、校验值、验证状态和错误信息。

## 当前状态

Godot 4 已实现安装探测：优先读取 `GODOT_BIN`，其次搜索 PATH 中的 `godot4` 和 `godot`。探测结果可通过 `gamebench engine-info` 查看。实体执行桥接与结果导入仍需在安装 Godot 后完成，因此当前不会出现在 Synthetic 工作负载列表中。

Source 2 已登记适配器契约。由于 Source 2 没有通用公开 SDK，GameBench 不扫描或猜测普通 Steam 游戏路径，只接受显式 `SOURCE2_BIN` 或 `SOURCE2_ROOT`。可识别 `resourcecompiler`、`cs2`、`dota2` 和 `hammer` 等本地工具布局；即使检测成功，也只标记为 `detected-only`，因为 Valve 尚未提供可在 GameBench 中调用的统一无头 Benchmark 接口。

Unreal Engine 5 与 O3DE 只登记接口和未实现状态，不提供虚假运行命令。

## Godot 接入约束

- 引擎必须使用无窗口或 headless 模式，避免 GPU 和显示服务器成为主要变量。
- 引擎项目必须记录版本、启动参数、场景、线程设置和插件版本。
- 固定种子、预热帧和采样帧数必须由 GameBench 配置传入。
- 结果必须包含引擎侧校验值；校验失败的结果不得评分。
- 不允许静默删除引擎异常样本。

## 后续实现

1. 在 Godot 项目中加入只读 Benchmark 插件和统一结果协议。
2. 通过无 shell 的子进程接口启动引擎并收集 stdout 结果。
3. 将 Engine 结果转换为独立 JSON schema，并与 Synthetic 结果分开比较。
4. UE5 与 O3DE 复用相同适配器接口，分别维护版本兼容矩阵。
5. Source 2 只有在获得合法工具链和稳定的无头结果协议后才进入 `detected-only` 之外的执行阶段；不得把启动游戏客户端的时间当作 CPU Benchmark。

