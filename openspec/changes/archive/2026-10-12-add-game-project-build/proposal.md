# Change: add-game-project-build

> 状态：proposal（未实现）。为「新建工程」生成可编译的 Game 源码工程与跨平台构建流程。

## Why

当前 `ProjectDescriptor::Create` 只生成 `assets/`、`configs/`、`cache/` 与 `<name>.skyproj`（`engine/framework/src/project/ProjectDescriptor.cpp`），**没有任何游戏代码编译流程**。用户新建 `TestProject` 后无处安放游戏逻辑，也没有把游戏代码编译成引擎可加载模块的工程。

引擎已具备承载该流程的基础设施：
- 运行时模块机制：`IModule` + `REGISTER_MODULE(Name)`（导出 `StartModule/StopModule`），由 `ModuleManager` 按 `configs/modules_game.json` 中的名字 `dlopen` 加载；
- 构建系统：`sky_add_library`/`sky_add_test`、`SKY_ENGINE_ROOT`、`3RD_PATH`、跨平台（Win32/macOS-x86/arm/Android/Linux，iOS 见 `add-ios-platform`）；
- 工程描述：`.skyproj` 已有 `modules`/`plugins` 字段。

缺的是「工程侧的源码目录约定 + 构建入口 + 模板脚手架」。

## What Changes

- **工程脚手架（创建时生成）**：新建工程时，除现有目录外生成：
  - `<Project>/source/`：Game 模块源码骨架（`CMakeLists.txt`、`include/`、`src/`，含 `<Name>Module.h/.cpp` 使用 `REGISTER_MODULE`，以及一个示例反射组件）；
  - `<Project>/CMakeLists.txt`：工程构建入口（引用引擎 + 加入 `source/`）；
  - `<Project>/CMakePresets.json`（或 `build.py`）：按平台配置/构建（复用引擎第三方与生成流程）；
  - 更新 `.skyproj` 的 `modules`（加入游戏模块 id）与工程 `configs/modules_game.json`。
- **引擎模板**：新增 `engine/templates/game/` 作为脚手架**唯一来源**，由创建流程拷贝并做名称替换。
- **工程构建**：工程 CMake 通过 `SKY_ENGINE_ROOT` 引用引擎（首版 `add_subdirectory`），产出**以工程命名的 SHARED 游戏模块**，链接引擎接口；平台覆盖与引擎一致（iOS 待 `add-ios-platform`）。
- **编辑器接入**：Project Manager 的创建动作调用脚手架（`ProjectDescriptor::Create` 扩展或新增 `ProjectScaffold` 助手）。
- **打包约束**：发布的工程包内容是**该平台的 cook 产物（product bundles）**，**不是** `assets/` 源文件；运行时只读 product bundle（引擎 `common`/`tex_*` + 工程 cooked bundle）。`--project <dir>` 属开发模式（读工程树/编辑器内按需 cook），非发布形态。详见 `docs/features/asset-pipeline.md`。

## Capabilities

### New Capabilities
- `game-project-build`: 新建工程生成 Game 源码工程（`source/`）与跨平台构建流程；产物为引擎可加载的 SHARED 模块。

### Modified Capabilities
- `editor-project-manager`: 创建工程的场景扩展为同时生成 `source/` 与构建入口，并在 `.skyproj`/模块列表中登记游戏模块。

## Impact

- `engine/framework/src/project/ProjectDescriptor.cpp`（或新增 `ProjectScaffold`）
- 新增 `engine/templates/game/**`
- `engine/sandbox/shell/src/ProjectManagerView.cpp`（创建动作）
- 工程侧 `CMakeLists.txt` / `CMakePresets.json` / `source/**`
- 打包：工程源资产 → cook → 逐平台 product bundle（`assets/` 源不进包）
- 文档：工程创建与构建指南
- 依赖：`editor-project-manager`（创建流程）、`thirdparty-build`（第三方）
