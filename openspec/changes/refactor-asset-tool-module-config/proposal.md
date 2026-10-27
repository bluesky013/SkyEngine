## Why

AssetTool 在 `WorkerApplication::LoadConfigs()` 中写死 builder 模块名与依赖，并用 `SKY_HAS_ANIMATION_BUILDER`、`SKY_HAS_JOLT_BUILDER` 控制注册。新增业务插件因此必须修改工具主程序，运行时清单又与构建、静态注册及部署清单分离。Jolt 独立提交验证中，构建 AssetTool 后仍需单独补建 CompressionModule 才能执行产品审计，暴露了这条依赖边界。

Game、Editor 已使用 `modules_*.json`，但解析代码各自维护；旧 ToolApplicationBase 还保留不同路径与宽松错误处理。应复用现有模块框架并统一必要的解析能力，而不是继续增加宿主专用宏。

## What Changes

- 新增 `configs/modules_asset_tool.json` 和最小 `configs/modules_asset_audit.json`，沿用 `modules / name / dependencies / buildOption` 格式。
- 将模块清单解析提取为 Framework 内的小型共用函数，复用 `ModuleInfo`、`ModuleManager::GetBuildOption()`、现有拓扑与生命周期；AssetTool、Game、Editor 共用解析逻辑，配置定位仍由宿主负责。
- 删除 AssetTool 中业务模块名清单及 `SKY_HAS_*_BUILDER` 宏；cook 与审计按运行模式选择配置。
- 让 AssetTool 的构建依赖、配置部署和静态工厂收集消费同一份清单；区分静态工厂可用性与本次运行的模块激活。
- 明确启用模块缺失、配置损坏和未知 buildOption 均显式失败；修正“缺失 DLL 被跳过”的过期注释。
- 保持 builder 的唯一 metadata owner、无界面 cook 和无源码产品审计边界。

## Capabilities

### New Capabilities

- `application-module-config`: 共用模块清单解析、宿主配置选择、构建/部署与运行时清单一致性。

### Modified Capabilities

- `asset-cook-ipc`: 将 worker builder 的能力来源改为配置，明确缺失启用模块时在启动阶段失败。

## Impact

- 涉及 `engine/framework/application`、Sandbox EditorApplication、`tools/asset_tool`、`configs`、现有 CMake 依赖/静态注册/部署 helper 与对应测试。
- 不新增 core、不新增 target、不搬迁模块、不另建插件发现器或配置服务。
- 不改变 cook 协议、资产 schema、物理材质/Shape/streaming 实现。
- 与已归档 `replace-bullet-with-jolt` 分开实施和提交；不阻塞其适配代码提交。

## Current Status

2026-10-27 用户要求先建立 change、后续再做。本次仅形成 proposal/design/specs/tasks；实现和运行时验收均未开始，不归档、不标记实现完成。
