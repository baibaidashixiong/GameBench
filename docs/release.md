# 发布检查清单

## 版本

- 当前版本：`0.1.0`。
- 结果 schema：`1`。
- 评分版本：`1`。

## 发布前验证

1. 在干净工作区执行 `cmake --preset release`。
2. 执行 `ctest --preset release --output-on-failure`。
3. 对涉及并发或内存的改动执行 ASan、UBSan 和 TSan Preset。
4. 验证无 Vulkan 与无 hwloc 的降级构建。
5. 使用 Release 二进制运行全部 Synthetic 工作负载并检查校验状态。
6. 用结果查看器加载 JSON，检查桌面与移动布局、图表和导出按钮。
7. 执行 `scripts/build-release.sh` 生成安装目录与 TGZ。

## 发布产物

- `gamebench` CLI。
- README、许可证、贡献指南、更新日志和全部文档。
- 示例配置。
- 离线结果查看器。
- `GameBench-0.1.0-Linux.tar.gz`。

## 已知限制

- Godot 仅完成安装探测与适配器接口，执行桥接尚未启用。
- UE5 与 O3DE 未实现。
- Jolt Physics 未安装，当前使用原生 Physics 后端。
- CCD 与 P/E Core 类型在当前平台报告 `Unknown`。
- 综合跨工作负载分数必须等待版本化参考数据集，当前只提供单工作负载透明指标。

