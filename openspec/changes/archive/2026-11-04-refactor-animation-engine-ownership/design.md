## Context

2026-11-02 用户批准将 `plugins/animation` 收拢到 `engine/animation`。迁移前算法 target `Animation` 仅依赖 Core 和可选 ACL；新增集成位于 plugin 四个子目录，共 44 个文件（包含入口 CMake）。2026-11-03 用户进一步批准将原有算法目录与 Graph 合入 core，资产另置 assets。本 change 是独立结构调整，`add-skeletal-animation-workflow` 继续拥有尚未完成的功能和平台验收。

## Goals / Non-Goals

Goals: 统一动画归属；保留算法独立、源码唯一 owner、metadata 唯一 owner、模块名称、公开 include、装载和裁剪契约。

Non-goals: 改变资产格式、求值、任务或 GPU 生命周期；移动 Content/Aurora；补齐 workflow 未完成的功能。

## Decisions

### 目录与依赖

所有动画实现归入 `engine/animation/{core,assets,adaptor,builder,editor}`，删除 plugin 根 CMake 与清单注册。原 `include/src/test` 迁入 core 并合入 GraphDefinition/GraphInstance；Graph 源文件位于 core/src/graph。统一算法 target 为 `Animation`，仅依赖 Core/可选 ACL，删除 `Animation.Graph.Static`。Graph 测试并入 `AnimationTest`，共用 core/test/main.cpp；ModuleTest 继续复用这个测试入口，删除独立 `Animation.GraphTest`。

资产源文件、公开资产头和 metadata 入口迁入 assets/{src,include,module}。GraphAssets 依赖 Animation/Framework，World 依赖 GraphAssets/Framework/Aurora.Renderer.Interface，Builder 依赖 GraphAssets/Framework，Editor 依赖 GraphAssets/Sandbox 编辑框架/Renderer.Interface。四个模块入口及名称保留；不把 Framework 资产接口引入算法库。

`engine/CMakeLists.txt` 在 Content、Aurora、Sandbox 及其编辑扩展配置完成后纳入动画集成，确保测试依赖和编辑框架 target 已存在。animation 根 CMake 早期配置 core/Animation；不让 `Animation` 反向依赖集成。

### 开关与装载

`cmake/options.cmake` 定义 `SKY_BUILD_ANIMATION`，默认 ON，仅控制迁入的集成 targets。OFF 时含 Graph 的 Animation 与 AnimationTest 仍可使用，GraphAssets/World/Builder/Editor 不配置。Builder 仍受 SKY_DEVELOP 约束，Editor 仍需 SKY_DEVELOP 和 SKY_BUILD_SANDBOX。

现有 plugin 开关 helper 继续负责 plugin 默认值；其生成表扩充为 engine 与 plugin 的 build options，私有生成头改为 `BuildOptions.h`。Animation 条目来自 engine 选项，不受 plugin 清单刷新影响。Framework 查询相同字符串；现有游戏/编辑器配置与项目模板不改名。静态和动态打包均保留四个模块入口。

### 契约归属

AGENTS 明确 Animation 与 Aurora/UI 一样允许在 engine 内部实现，保留分层和跨模块接口约束；不扩大到其它业务 feature。主 spec 与活跃 workflow 的路径/owner 同步更新，历史归档和历史提交清单保持原始记录。

## Risks / Trade-offs

- 配置顺序导致缺依赖 → 集成放在 engine 配置尾部、SandboxEditor 依赖绑定之前，配置真实库/模块/测试。
- 删除 plugin 条目丢失运行时开关 → 回归默认 ON、显式 OFF、cache 重配置和 plugin 清单变更，检查生成表及构建图。
- 单测通过但模块无法装载 → 构建真实模块与 AssetTool，运行现有 ModuleTest；平台失败须记录实际边界。
- 本机为 macOS/Xcode → 保留现有 cache 的 generator/3RD_PATH；不把 macOS 结果声明为 VS2022 验收。

## Migration Plan

先记录原始文件与 targets，再合并 core 并拆出 assets、切换 CMake/开关表、更新契约和引用，最后配置构建并运行回归。无需数据迁移或 recook。回退可逆向移动并恢复清单与 CMake；不更改用户资产。
