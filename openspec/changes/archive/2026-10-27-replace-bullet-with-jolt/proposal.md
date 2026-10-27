## Why

SkyEngine 当前只交付 Bullet Fast 后端，且 SDK 实现、World 接入和组件元数据集中在 Bullet plugin。此次按用户要求直接切换至 Jolt：以一个明确的物理提供方承接现有引擎契约，移除 Bullet 源码、依赖和装载入口，避免维护两套后端。

## What Changes

- **BREAKING**：删除 `plugins/bullet`、`3rdParty::bullet3`、`SKY_BUILD_BULLET`、`BulletPhysicsModule` 和 `BulletPhysicsRenderModule`；不保留双后端切换或 Bullet 兼容模块。
- 引入固定版本 Jolt v5.6.0，使用现有 third-party bootstrap；新 provider 放在 `plugins/jolt`，保留 `engine/physics` 的接口/数据边界。
- 由 adaptor target 统一承担 SDK 后端与 Framework 对接，render target 独立承担 Aurora mesh bridge；不设无独立业务职责的 core。详细依赖和装载替换表见 design.md，用户已确认简化方案。
- 保留 `Physics` 子系统名、`sky::phy::PhysicsBodyComponent` / `PhysicsBodyData` / `PhysicsSubSystemConfig` 的类型身份及既有序列化字段；补齐描述数据 round-trip，组件只通过 `IPhysicsSystem` 访问世界。
- 实现既有刚体、七类 shape、五类约束、character、32-bit filter、material、query、event、debug、snapshot 契约；明确修正旧约束 limits/motor 被忽略、Distance 被实现为点约束的问题。
- 对 Jolt 显式失败结果和固定容量耗尽提供引擎 Status；统一 Tick/SingleStep 的错误、事件和插值路径。Fast 仍不宣称跨平台 Exact 或 rollback replay。
- 更新默认 plugin 配置、游戏模板、打包测试和当前使用文档；保留历史 archive 和技术比较中的历史事实。

## Capabilities

### New Capabilities

- `jolt-integration`: 固定 SDK 构建、唯一提供方、模块所有权、SDK/世界生命周期、删除范围和迁移验收。

### Modified Capabilities

- `physics-backend`: 由 Jolt 实现形状扩展；显式容量/初始化错误；活跃世界存在时不得销毁其 SDK 状态。
- `physics-material`: 将 Bullet 专属实现要求替换为 Jolt material/contact 映射，明确 combine 与阻尼行为。
- `physics-stepping`: Step 返回状态；Tick 与 SingleStep 使用同一完成路径；失败世界停止推进，内部 job 在返回前收齐。
- `physics-constraints`: 明确 limits、motor 模式、Distance、world anchor 和无效句柄的语义，禁止创建功能不完整的关节。
- `physics-serialization`: 保持已有物理类型身份，补齐复合形状/高度样本/凸包点的序列化，元数据只在唯一 definition owner 注册。

## Impact

- 实现：`plugins/bullet/**` 替换为 `plugins/jolt/**`；`engine/physics` 增加结果/容量描述并调整调用接口，消费方和测试同步适配。
- 构建：`cmake/thirdparty.json`、`cmake/thirdparty/Findbullet3.cmake`、`plugins/plugins.json`、游戏模块模板、`python/tests/create_package_smoke.py` 以及生成的运行时模块清单。
- 行为：旧场景的物理身份保留，但求解器响应不承诺逐帧一致；Distance 和 previously ignored limits/motors 会开始执行声明的语义。项目自行维护的模块配置必须显式改成 Jolt 名称。
- 本次不接入软体、车辆、ragdoll authoring、双精度世界或网络 Exact。后续设计可消费 Jolt，但不得把本次刚体迁移当作这些能力已经交付。
- 当前工作区已有未提交的其他变更。实现只修改此迁移必要的行；不重置、归档或提交其他工作。
