## Why

Animation 的算法实现已位于 `engine/animation`，而 Graph、World、Builder、Editor 分散在 `plugins/animation`。用户确认将动画作为完整 engine 子系统收拢，并于 2026-11-03 批准将原有算法目录与 Graph 统一到 core；模块化和可裁剪不要求注册为业务 plugin。

## What Changes

- 将动画实现统一到 `engine/animation/{core,assets,adaptor,builder,editor}`；原有 include/src/test 与纯 Graph 合入 core，统一由 `Animation` / `AnimationTest` 编译。删除 `Animation.Graph.Static` / `Animation.GraphTest`，保持模块名和 `animation/...` include。
- 将 `SKY_BUILD_ANIMATION` 定义为默认开启的 engine 集成开关，保留含 Graph 的算法库始终可用和集成功能可裁剪的行为。
- 保留运行时 buildOption 查询、静态/动态模块装载及 metadata 唯一 owner。
- 更新 AGENTS 和当前动画 workflow 的归属契约；本 change 仅负责结构迁移，不接管 workflow 未完成的功能验收。

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `modules`: 明确 Animation 属于 engine 子系统，内部保留单向依赖和独立实现 target。
- `build`: engine 动画集成开关、配置顺序和运行时开关表独立于 plugin 注册。
- `assets`: Graph 的 schema/handler/builder 唯一归属 engine Animation 对应 target。

## Impact

`engine/animation`、`engine/CMakeLists.txt`、`plugins/plugins.json`、`cmake/options.cmake`、运行时生成的 build options、Framework 查询入口、构建回归、AGENTS 和活跃 OpenSpec 引用。模块配置中的名字、资产身份和格式不变；不搬迁 Content/Aurora，不修改动画算法。
