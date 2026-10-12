## 1. Cook selection model

- [x] 1.1 扩展 `CookConfig`：bundle 选择按**发行档（distribution，= export preset 名）**组织（新增 `distributions` 解析 + `GetDistributionBundles`）
- [ ] 1.2 质量分档用**不同 bundle 名**（`tex_low`/`tex_hd`）——约定已入文档；示例 cook 配置待补（`engine/configs` 目前为空）
- [~] 1.3 解析"某档 → bundles"：C++ 侧 `GetDistributionBundles` 已提供；**脚本**当前用 `export_presets.json.bundles` 显式覆盖，尚未回退读 cook 配置的 distribution（JSONC 注释解析待做）

## 2. Packaging (python/package_project.py)

- [x] 2.1 `package`/`generate` 前 **cook**：**复用既有** `tools/asset_tool`（`AssetTool`）——给它加一次性 `--cook-all --platform <p>` 模式（沿用其既有 project/engine 引导 + builder 模块加载 + `SetForceInProcess`），遍历源 → `CookConfig::GetTargets` → `BuildRequestSync` → `<Project>/products/`；脚本经 `run_cook()` 调 `AssetTool --cook-all`。实测：`AssetTool --cook-all` 运行成功（空工程 0 条）；**带真实资产的验证待补**
- [x] 2.2 只挑选该档的 bundle 目录，按平台落位：Android → gradle `assets/products/<bundle>/`（实测进 APK）；桌面/iOS → `<Project>/build/<platform>/bin/products/`（best-effort，exe 目录/`.app Contents/Resources` 的最终对齐待 cook 接入后收口）
- [x] 2.3 写 `products/bundles.json` 清单（`{platform,distribution,bundles:[…]}`），与拷贝同一步生成
- [ ] 2.4 冲突校验（同 uuid/同名）未实现

## 3. Runtime mounting

- [x] 3.1 `GameApplication` 按平台解析 products 根（桌面/iOS `GetBundlePath()/products`；Android APK assets `products`，经 `GetBundleFileSystem()`）
- [x] 3.2 读 `products/bundles.json`，逐个 `AddAssetProductBundle`（`HashedAssetBundle`，`CreateSubSystem("products/<b>")`）
- [x] 3.3 移除硬编码 `common`；清单缺失时告警并回退 `common`（不崩溃）；`--project` 开发模式跳过产品 bundle
- [x] 3.4 与 `AssetManager::OpenFile` 首个命中语义兼容（同档内 bundle 名不同、无 uuid 重叠）

## 4. Verification & docs

- [~] 4.1 单测：`CookConfigTest.BundlesAndPresets` 覆盖 distribution→bundles；运行时装挂载未单测（需 FS+bundle fixture）
- [~] 4.2 端到端：Android `generate`/`package` 实测 APK 含 `assets/products/bundles.json`（内容正确）；"两个 Android 档(normal/HD) 各出包"未做
- [ ] 4.3 文档：`docs/features/game-project-build.md`、`asset-pipeline.md` 待补（bundle 入包 + `bundles.json` + 运行时挂载）
