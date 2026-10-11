## 1. Independent CMake entry

- [x] 1.1 `native/CMakeLists.txt`: `set(ENGINE_ROOT)` → include `cmake/{functions,options,plugin_switches,configuration,thirdparty}.cmake` → `add_subdirectory(${ENGINE_ROOT}/engine)` + `add_subdirectory(${ENGINE_ROOT}/plugins)`
- [x] 1.2 Per-platform output dir: set `<Project>/build/<platform>/bin` on desktop (multi-config includes `$<CONFIG>`); do NOT override `CMAKE_*_OUTPUT_DIRECTORY` when `ANDROID`
- [x] 1.3 End of entry: `sky_set_dependency(TARGET Launcher)`
- [x] 1.4 Retire legacy `cmake/template/{CMakeLists_project,CMakeLists_native}.txt`

## 2. Engine launcher removal

- [x] 2.1 Delete `engine/launcher/**` (android java/res/manifest + ios/macos/windows entries) after migrating sources into templates
- [x] 2.2 `engine/CMakeLists.txt`: drop `add_subdirectory(launcher)`, `sky_set_dependency(TARGET Launcher)`, and the `if (TARGET Launcher)` sandbox block
- [x] 2.3 Root `CMakeLists.txt`: drop `sky_set_dependency(TARGET Launcher)`
- [x] 2.4 Verify no `Launcher` target is defined by a standalone engine configure（已重配 `cmake-build-debug` + `FrameworkTest` 构建通过）

## 3. Templates and launcher ownership

- [x] 3.1 Add `engine/templates/launcher/{android,ios,macos,windows}` (cpp/main + Android java/res/manifest) with tokens `{{PROJECT_NAME}}`/`{{PROJECT_DISPLAY_NAME}}`/`{{ANDROID_PACKAGE}}`/`{{ANDROID_PACKAGE_PATH}}`/`{{LAUNCHER_LIB_NAME}}`
- [x] 3.2 Restructure `engine/templates/game` to the `native/` layout (CMake entry + `source/`)
- [x] 3.3 `native/` defines the `Launcher` target per platform with `OUTPUT_NAME "<Name>"` (+ iOS `PRODUCT_NAME`, macOS `.app` from preset)
- [x] 3.4 Project `Launcher` deploys configs (engine first, then project) via a custom target; `templates/` deploy pending（templates 部署待 P4 补）

## 4. Packaging tool and IDE projects

- [x] 4.1 `python/package_project.py create`: scaffold `game` + `launcher` templates into `native/` (application id → Android package path); keep presets/credentials
- [x] 4.2 `generate`/`package`: outputs under `<Project>/build/<platform>`; `templates/ide/android` references `native/` (`externalNativeBuild` + `java/res.srcDirs`); Android params (ABI/SDK/NDK) from preset（ios/macos 改为 `cmake --preset`；移除 obsolete `templates/ide/{ios,macos}`）
- [x] 4.3 Android artifact `lib<Name>.so` with tokenized `loadLibrary` + manifest `lib_name`（本机实测 APK 含 `libMyGame.so` + `libMyGameModule.so`）

## 5. Editor integration

- [x] 5.1 Project Manager New / platform actions shell out to the CLI; clear error when Python is unavailable（`CreateProjectFromResult` 调 `python/package_project.py create`，`SKY_PYTHON`/`SKY_ENGINE_ROOT` 定位；开 `SKY_BUILD_SANDBOX=ON` 后 `SandboxModule`+`SandboxEditor` **编译通过**）
- [x] 5.2 Remove C++ `ProjectScaffold` (+ its test) and the `ProjectDescriptor::Create` scaffold coupling（删除 3 文件；`ResolvedApplicationId` 内联 sanitize；`FrameworkTest` 构建通过）

## 6. Verification and docs

- [x] 6.1 Android: `create` → `package --preset <android>` produces an APK with `lib<Name>.so` (local test) —— 实测 `MyGame-debug.apk` 含 `libMyGame.so`+`libMyGameModule.so`
- [x] 6.2 macOS 全量构建：`cmake --build --preset macos --target Launcher` **BUILD SUCCEEDED** → `build/macos/bin/Release/MyGame`（产物名=工程标识符）+ `libMyGameModule.dylib` + 引擎/插件 dylib；`MyGameDeploy` 部署 configs（`modules_game.json` 为工程版本，含 `MyGameModule`）；运行 `MyGame` 能启动、加载引擎模块配置与 `Aurora.Render` 并进入 Vulkan 设备初始化（本机无显示/GPU 上下文，未跑到帧循环）；Windows/iOS 仍未验证
- [x] 6.3 Update `README.md`（运行段/字段）、`.opencode/.claude/.github` skills（build/architecture/cpu）、`docs/features/game-project-build.md`、`docs/editor/editor-framework-design.md`（加注）、`add-ios-platform`（launcher 模板路径）；`AGENTS.md` 无需改
- [x] 6.4 预设打包选项：`bundleId`/`versionName`/`versionCode`、Apple `signing(auto|off)`/`bundle`/`team`/`signingIdentity`/`provisioningProfile`、`cacheVariables`/`compileDefinitions`/`compileFlags`/`extraConfigureArgs`/`extraBuildArgs` 经 CLI 翻译为 cmake cache 参数；模板支持 macOS `.app`（`SKY_MACOS_BUNDLE`）+ `Info.plist`；实测 macOS→`MyGame.app`(`CFBundleIdentifier=com.skyengine.mygame`)、iOS→`MyGame.app`（预设 `signing:off` 免签名构建）

## 7. Project Manager：预设配置 UI（反射面板，AGENTS 规则）

- [x] 7.1 预设数据对象反射注册：`framework/project/ExportPreset.{h,cpp}`（`ExportPreset` + 整份 `ExportPresets`），`RegisterExportPresetReflection()` 静态一次注册（framework 侧，非 ui）
- [x] 7.2 读写 `export_presets.json`：`ExportPresets::Load/Save`（与 CLI/脚本同 schema）；编辑经 `ReflectedFormView`（内部走 `CommandService` 可撤销），`SetOnEdited` → 保存到 `export_presets.json`
- [x] 7.3 Project Manager/编辑器视图：`PresetEditorPanel : ReflectedFormView`（绑定整份 `ExportPresets`，序列成员渲染全部预设，不手搓表单）；`EditorShell` 注册 `"export-presets"` 面板视图 + `SetExportPresets`；`DefaultPanels` 注册；`SandboxModule` 打开工程时载入并 `Tabify` 到 Outliner 停靠
- [x] 7.4 与 CLI 同一 schema（`framework/project/ExportPreset` ↔ `python/package_project.py` 的 `export_presets.json`）；凭据（口令/团队）仍在 `export_credentials.json`/env，不在 UI 明文
  - 验证：`ExportPresetTest`（JSON 往返 + 反射节点）通过；`FrameworkTest`+`SandboxModule`+`SandboxEditor` 均构建成功
