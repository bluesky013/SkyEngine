## 1. Cook selection model

- [ ] 1.1 扩展 `CookConfig`：bundle 选择按**发行档（distribution，= export preset 名）**组织（在现有 platform-keyed `presets` 基础上加 `distributions`）
- [ ] 1.2 质量分档用**不同 bundle 名**（`targets: tex→tex_low`、`tex_hd→tex_hd`）；更新示例 cook 配置
- [ ] 1.3 提供"某档 → bundles"的解析（导出 preset 的 `bundles` 显式覆盖，否则取 cook 配置的 distribution）

## 2. Packaging (python/package_project.py)

- [ ] 2.1 `package` 增加 **cook**（或要求已有 `products/`）：按该档的 bundle 目标烘焙
- [ ] 2.2 只挑选该档的 bundle 目录，按平台落位：
  - Android：APK `assets/products/<bundle>/`（`app/build.gradle` 加 `sourceSets.main.assets.srcDirs` 指向该档 bundle 暂存目录）
  - 桌面/iOS：`products/<bundle>/` 到 launcher 旁（`.app` 为 `Contents/Resources/products/`）
- [ ] 2.3 写 `products/bundles.json` 清单（`{platform,distribution,bundles:[…]}`），与拷贝同一步生成
- [ ] 2.4 校验：选中的 bundles **无同名/同 uuid 冲突**，否则报错

## 3. Runtime mounting

- [ ] 3.1 `GameApplication`：按平台解析 products 根（桌面/iOS `GetBundlePath()/products`；Android APK assets `products`）
- [ ] 3.2 读 `products/bundles.json`，逐个 `AddAssetProductBundle`（Android 用 `AndroidBundleFileSystem`/asset FS；桌面/iOS 用 NativeFileSystem 子目录）
- [ ] 3.3 移除硬编码 `common`；清单缺失时告警并回退 `common`（不崩溃）
- [ ] 3.4 确认与 `AssetManager::OpenFile` 首个命中语义兼容（同档内无 uuid 重叠）

## 4. Verification & docs

- [ ] 4.1 单测：清单解析 + 按清单挂载 + 缺失回退
- [ ] 4.2 端到端：一个工程两个 Android 档（normal/HD）各出 APK，各自只含对应 bundle；桌面出包后旁挂 `products/`
- [ ] 4.3 文档：`docs/features/game-project-build.md`（资产的入包与挂载）、`docs/features/asset-pipeline.md`（distribution 选择 + `bundles.json`）
