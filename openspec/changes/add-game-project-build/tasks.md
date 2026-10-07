# Tasks: add-game-project-build

## 1. Templates

- [ ] 1.1 `engine/templates/game/`：`CMakeLists.txt`、`CMakePresets.json`、`source/`（`CMakeLists.txt` + `include/` + `src/` + 示例组件）、`configs/modules_game.json`
- [ ] 1.2 令牌占位（`{{PROJECT_NAME}}`、`{{MODULE_ID}}`）与替换规则

## 2. Scaffolding

- [ ] 2.1 `ProjectScaffold`：拷贝模板 + 替换令牌到 `<Project>/source` 等
- [ ] 2.2 接入 `ProjectDescriptor::Create`（创建即生成源码工程与构建入口）
- [ ] 2.3 更新 `.skyproj` 的 `modules` 与工程 `configs/modules_game.json`

## 3. Project build

- [ ] 3.1 工程 `CMakeLists.txt`：引用引擎（`SKY_ENGINE_ROOT` + `add_subdirectory`）+ `source/`
- [ ] 3.2 `source/CMakeLists.txt`：`sky_add_library(TARGET <Name>Module SHARED ...)`，链接引擎接口
- [ ] 3.3 `CMakePresets.json`：各平台 configure/build（`3RD_PATH`、生成器）
- [ ] 3.4 平台覆盖：Win32 / macOS(x86/arm) / Linux / Android（iOS 待 `add-ios-platform`）

## 4. Runtime

- [ ] 4.1 `<Name>Module : IModule` + `REGISTER_MODULE`；示例反射组件注册
- [x] 4.2 启动器 `--project` 挂载工程目录为 workFs 并加载工程模块（macOS 验证：Project mounted + TestProjectModule 加载成功）

## 5. Editor

- [ ] 5.1 Project Manager 创建动作调用脚手架并回显结果（失败处理）

## 6. Docs / Verification

- [ ] 6.1 `docs/`：新建工程与跨平台构建指南
- [x] 6.2 端到端（macOS）：模板生成 `TestProject` → 编译出 `libTestProjectModule.dylib` → `Launcher --project` 挂载并加载模块
- [ ] 6.3 至少双平台验证（如 macOS + Win32）
