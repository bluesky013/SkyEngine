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
- [x] 3.3 `CMakePresets.json`：macos/windows/linux/android/ios configure+build presets（引擎开关 SKY_BUILD_*）
- [ ] 3.4 平台覆盖：Win32 / macOS(x86/arm) / Linux / Android（iOS 待 `add-ios-platform`）
  - Win32：**预留**，模板 `windows` preset 与引擎 Win32 后端已具备；构建/运行验证由 Windows PC 上后续完成
  - macOS：本机已验证；Android：需 `ANDROID_HOME` + Android 三方；iOS：待 `add-ios-platform`

## 4. Runtime

- [ ] 4.1 `<Name>Module : IModule` + `REGISTER_MODULE`；示例反射组件注册
- [x] 4.2 启动器 `--project` 挂载工程目录为 workFs 并加载工程模块（macOS 验证：Project mounted + TestProjectModule 加载成功）

## 5. Editor

- [ ] 5.1 Project Manager 创建动作调用脚手架并回显结果（失败处理）

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
- [ ] 8.1 **可定制 bundle id**：工程配置（`.skyproj` settings）→ Android `applicationId` / Apple `CFBundleIdentifier`（含 namespace/包名一致性）
- [ ] 8.2 **签名**：Android keystore（`signingConfigs`/`storeFile`/密码，勿入库）与 Apple signing（`DEVELOPMENT_TEAM`/provisioning/codesign）
- [ ] 8.3 **生成 Android Studio 工程**：由模板生成/参数化 gradle 工程（`applicationId`/`namespace`/ABI/签名），可 `open in Android Studio`
- [ ] 8.4 **生成 Xcode 工程**：iOS/macOS 由 CMake `Xcode` 生成 `.xcodeproj`（bundle id/team/signing/entitlements 可配）
- [ ] 8.5 与打包联动：同一工程配置驱动「构建 → 生成 IDE 工程 → 签名 → 产出（APK `.app`）」
