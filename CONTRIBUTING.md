# 贡献指南

## 基本要求

1. 使用 C++23，遵循项目 `.clang-format` 与 `.clang-tidy` 配置。
2. 所有面向用户的文档、提交说明和主要代码注释使用简体中文；标识符使用英文。
3. 新工作负载必须支持固定随机种子、正确性验证和可配置规模。
4. 禁止提交 `.build/`、临时结果、机器专属配置或未通过测试的代码。
5. 性能改动必须提供 Release 构建下的前后测量方法，禁止仅凭推测优化。

## 开发流程

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
cmake --build --preset release
ctest --preset release
```

提交信息使用 Conventional Commits，例如 `feat(runtime): 实现 Work Stealing 调度`。

