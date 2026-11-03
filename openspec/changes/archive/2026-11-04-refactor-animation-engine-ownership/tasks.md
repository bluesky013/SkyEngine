## 1. 目录与构建

- [x] 1.1 将算法 include/src/test 与 Graph 合入 core/Animation，资产实现移入 assets，保持公开 include 和模块名称。
- [x] 1.2 从 engine 按依赖顺序配置集成 targets，删除动画 plugin 注册。
- [x] 1.3 将功能开关归入 engine，生成并查询包含 engine/plugin 的运行时开关表。

## 2. 契约与回归

- [x] 2.1 更新 AGENTS、领域主规范与活跃 workflow 的归属/路径；保留历史记录和未完成验收。
- [x] 2.2 验证开关默认值、显式禁用、cache/manifest 变化、构建分层和静态/动态模块入口。
- [x] 2.3 运行 CMakeSourceOwnership 并配置/构建受影响真实库、模块与测试。
- [x] 2.4 运行动画与打包回归，记录本机结果及不能完成的环境验收。
- [x] 2.5 验证 OpenSpec 和最终 diff，并交付 review；2026-11-04 用户确认后归档，纳入实现的同一次提交。
