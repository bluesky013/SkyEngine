## 1. Convention & options

- [x] 1.1 普查：现状模块 = 单 SHARED + `REGISTER_MODULE`（无 static/dynamic 拆分）→ 采用 **tagged 宏 + 生成注册 cpp** 方案
- [x] 1.2 打包模式：`SKY_MODULE_PACKAGING=dynamic|static`（默认 dynamic）；静态集合 = 经 `sky_add_module` 声明（=编译选择/plugin config 的体现）的模块目标
- [x] 1.3 引擎选项 `SKY_MODULE_PACKAGING`（`cmake/options.cmake`）；static 时全局 `SKY_MODULE_STATIC`
- [x] 1.4 `IModule.h` `REGISTER_MODULE_TAGGED(tag, name)`（static→唯一工厂 `sky_module_create_<tag>()`；dynamic→回退 `REGISTER_MODULE`）

## 2. Engine build support

- [x] 2.1 `sky_add_module()`（按选项出 SHARED/STATIC；登记 `SKY_STATIC_MODULE_TARGETS`）+ `ModuleManager::AddStaticModule`
- [~] 2.2 模块迁移到 `sky_add_module` + `REGISTER_MODULE_TAGGED`：**已迁** `Aurora.Render` / `Aurora.ShaderCompiler` / `AudioModule` / `RecastNavigation` / `BulletPhysicsModule` / `BulletPhysicsRenderModule` / `CompressionModule` / `ConsoleModule` / `EnetNetwork` / `PVS` / `XRModule` / `PythonModule` / `Content.Module`；**待迁** terrain/vegetation（运行时模块 id 与 target 名不一致，需先核对）+ editor 专属（按 2.6 不迁）
- [x] 2.3 生成器 `sky_generate_static_modules()`（configure 期按 `SKY_STATIC_MODULE_TARGETS` 产出 `RegisterStaticModules.cpp`；tag 名非法字符 `.`/`-` → `_`）
- [~] 2.4 集合来源：**生成基于 `sky_add_module` 目标**（不再需要单独的 `SKY_STATIC_MODULES` 列表 / `engine/runtime_modules.json`）；`plugin.json` 的 `kind` 未采用（保留为后续可选元数据）
- [x] 2.5 tag 规则：target 名（`. `/`-`→`_`）唯一，避免多模块符号冲突
- [x] 2.6 编辑器分层：仅 runtime 模块用 `sky_add_module` → editor/`requires_editor` 模块天然不进 runtime 静态集合（**约定 + 文档**；若改配置驱动选择再按 `kind` 过滤）

## 3. Runtime resolution

- [x] 3.1 静态优先、不重复注册（`AddStaticModule` + `LoadModules` 跳过 dlopen）
- [x] 3.2 诊断：`AddStaticModule` 重复注册告警（保留首个）；静态缺失时明确报错（提示 `sky_add_module`+`REGISTER_MODULE_TAGGED`+模块配置）；动态加载失败附 `GetLastError()`（见 design D7）
- [x] 3.3 环境：静态模块与宿主同进程共享 `Environment`（无需 Attach/Detach）；`Application::Init`（`SKY_MODULE_STATIC`）调 `SkyEngineRegisterStaticModules`

## 4. Packaging integration

- [x] 4.1 `export_presets.json` 增 `packing: dynamic|static`（`ExportPreset` 字段 + 序列化 + 反射）
- [x] 4.2 透传：桌面 → cmake `-DSKY_MODULE_PACKAGING=<packing>`；Android → gradle `externalNativeBuild` arguments（`{{MODULE_PACKAGING}}`）
- [x] 4.3 iOS/受限平台：预设可设 `packing: static`
- [x] 4.4 项目 `native/CMakeLists.txt`：static 时 `sky_generate_static_modules()` + `target_sources(Launcher ...)` + `target_link_libraries(Launcher ${SKY_STATIC_MODULE_TARGETS})`

## 5. Verification & docs

- [x] 5.1 单测：`ModuleManagerStaticTest`（静态模块 load/start，不 dlopen）—— 通过
- [~] 5.2 端到端：`MyStatic`（packing=static）**全量构建 + 链接 SUCCEEDED**（静态模块 + 生成注册链入工程 Launcher）；dynamic 路径零回归（FrameworkTest 通过）。**运行时装载未跑通**：macOS `.app` 的 `configs` 未随包（部署路径问题，与静态无关）
- [ ] 5.3 文档：模块打包指南（选项、生成注册、迁移步骤、平台约束），更新 `docs/features/game-project-build.md`
