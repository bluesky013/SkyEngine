# Tasks: add-game-project-build

## 1. Templates

- [ ] 1.1 `engine/templates/game/`：`CMakeLists.txt`、`CMakePresets.json`、`source/`（`CMakeLists.txt` + `include/` + `src/` + 示例组件）、`configs/modules_game.json`
- [x] 1.2 令牌占位（`{{PROJECT_NAME}}`，模块名由模板自行追加 `Module` 后缀）与替换规则

## 2. Scaffolding

- [x] 2.1 `ProjectScaffold`：拷贝模板 + 替换令牌到 `<Project>/source` 等
- [x] 2.2 接入 `ProjectDescriptor::Create`（创建即生成源码工程与构建入口）
- [x] 2.3 更新 `.skyproj` 的 `modules` 与工程 `configs/modules_game.json`

## 3. Project build

- [x] 3.1 工程 `CMakeLists.txt`：引用引擎（`SKY_ENGINE_ROOT` + `add_subdirectory`）+ `source/`
- [x] 3.2 `source/CMakeLists.txt`：`sky_add_library(TARGET <Name>Module SHARED ...)`，链接引擎接口
- [x] 3.3 `CMakePresets.json`：macos/windows/linux/android/ios configure+build presets（引擎开关 SKY_BUILD_*）
- [ ] 3.4 平台覆盖：Win32 / macOS(x86/arm) / Linux / Android（iOS 待 `add-ios-platform`）
  - Win32：**预留**，模板 `windows` preset 与引擎 Win32 后端已具备；构建/运行验证由 Windows PC 上后续完成
  - macOS：本机已验证；Android：需 `ANDROID_HOME` + Android 三方；iOS：待 `add-ios-platform`

## 4. Runtime

- [ ] 4.1 `<Name>Module : IModule` + `REGISTER_MODULE`；示例反射组件注册
- [x] 4.2 启动器 `--project` 挂载工程目录为 workFs 并加载工程模块（macOS 验证：Project mounted + TestProjectModule 加载成功）

## 5. Editor

- [x] 5.1 Project Manager 创建动作调用脚手架并回显结果（失败处理）

## 6. Docs / Verification

- [ ] 6.1 `docs/`：新建工程与跨平台构建指南
- [x] 6.2 端到端（macOS）：模板生成 `TestProject` → 编译出 `libTestProjectModule.dylib` → `Launcher --project` 挂载并加载模块
- [ ] 6.3 至少双平台验证（如 macOS + Win32）

## 7. Packaging（cooked products）

- [ ] 7.1 明确**打包内容 = cook 产物**（product bundles：`product.index` + `<uuid>.bin`，逐平台），**不含** `assets/` 源文件
- [ ] 7.2 打包步骤：对工程源资产跑 cook（`asset_cook`/`CookConfig` 目标）产出该平台 product bundle，随游戏模块/引擎一起分发
- [ ] 7.3 运行时（发布）：装载 **product bundles**（引擎 `common`/`tex_*` bundle + 工程 cooked bundle），**不挂载**原始 `assets/`（`--project` 开发模式除外）
- [ ] 7.4 文档记录该约束（源资产→cook→product 打包；运行时只读 product）

## 8. Platform packaging & IDE project generation（后续）

- [x] 8.0 legacy Python 项目/GUI 工具已退役删除（`project_manager.py`/`setup.py`/`third_party_gui.py`/`build/`/`widgets/`），统一到原生 ProjectScaffold + 模板
- [x] 8.1 **可定制 bundle id**：工程配置（`.skyproj` settings `applicationId`）→ Android `applicationId`/`namespace` 与 Apple `CFBundleIdentifier`；未配置时由工程名派生 `com.skyengine.<name>`（`ProjectDescriptor::ResolvedApplicationId`）
- [x] 8.2 **导出预设 + 凭据分离**（对标 Godot）：`export_presets.json`（入库：platform/variant/bundleId/abis/keystore/team）、`export_credentials.json`（不入库，`.gitignore`；口令/团队/描述文件），凭据解析 **env > file**（`SKY_ANDROID_KEYSTORE`/`SKY_ANDROID_STORE_PASSWORD`/`SKY_ANDROID_KEY_ALIAS`/`SKY_ANDROID_KEY_PASSWORD`/`SKY_APPLE_*`）；`.skyproj` 只留默认 identity（applicationId/version）
- [x] 8.3 **生成 Android Studio 工程**：`engine/templates/ide/android/`（`applicationId`/`namespace`/ABI/`signingConfigs.release`/内置 gradle wrapper），可 `open in Android Studio`
- [x] 8.4 **生成 Xcode 工程**：`engine/templates/ide/{ios,macos}/CMakePresets.json`（Xcode 生成器 + bundle id/team），运行 preset 产出 `.xcodeproj`；launcher 读 `SKY_BUNDLE_ID`/`SKY_APPLE_TEAM_ID`/`SKY_APPLE_SIGNING_IDENTITY`/`SKY_APPLE_PROVISIONING_PROFILE`
- [x] 8.5 **工具（Python 脚本）**：`python/package_project.py`（`create`/`generate`/`package` 子命令）；模板 `engine/templates/{game,ide}/` 为唯一来源
- [x] 8.6 **Android 打包（脚本）**：`package --preset <name>` → 实例化 → gradle wrapper `:app:assemble<variant>` → APK 到 `<Project>/build/<platform>/<Name>-<variant>.apk`
  - 本机已实测：`create` → `generate` → `package` 全链路出 debug APK（含 `libLauncher.so`），凭据经 `export_credentials.json` 注入 `signingConfigs.release`
  - 需 `ANDROID_HOME`/`ANDROID_NDK_HOME`/`SKY_3RD_PATH` + Gradle 兼容 JDK（17/21）
  - 后续：用真实 keystore 出 release 签名包
  - 目录布局（`build/<platform>`、`native/`、launcher 归属）由 `add-project-native-layout` 定义
- [ ] 8.7 **iOS 打包（IPA）**：依赖 `add-ios-platform`（iOS 三方/launcher）；`package --preset <ios>` `--configure` 出 `.xcodeproj` → `xcodebuild archive/exportArchive`
  - 后续：Apple 签名实值（team/provisioning）接入

## 9. Project layout / engine entry / launcher ownership

Moved to the change **`add-project-native-layout`** (the `native/` layout, independent CMake entry, project-owned
launcher, artifact naming, and `build/<platform>` outputs). Static/dynamic module *bundling* is a further change,
**`add-static-module-bundling`**.
