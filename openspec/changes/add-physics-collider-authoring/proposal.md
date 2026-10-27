## Why

状态（2026-10-27）：物理材质/Collision 产品、显式 World cell 加载、生命周期回归和 benchmark 已实施；专用制作工具、空间调度、Terrain 接线及扩大性能/平台验证仍待后续。已交付证据见 verification.md / benchmarks.md；剩余工作及验收映射见 [remaining-work.md](remaining-work.md)。本 change 保持开放，不将全部制作能力宣称完成。

Jolt 迁移提供了运行时形状与物理接口，但美术和关卡制作仍缺少可复用的碰撞资产、从模型生成碰撞体、视口编辑和可撤销的制作流程。需要在已有 Sandbox 编辑框架与资产 cook 管线中完成这些能力，避免每个模型在场景中重复手工配置。用户进一步要求考虑材质、场景持久化和 streaming；仅在进入关卡前预热不能覆盖运行时分块加载、激活及卸载。

## What Changes

- 设计独立 Collision 资产：保存来源、制作配方和稳定的子形状标识，发布可复现的几何产品。
- 提供 Box / Sphere / Capsule 拟合、单凸包、组合形状、静态 Triangle Mesh，以及后续 CoACD 多凸包分解。
- 增加碰撞资产编辑窗口、场景组件绑定、视口形状操作、预览物理 world 和预算诊断；属性与操作复用反射、CommandService、EditorActionRegistry。
- 明确子形状旋转、静态网格限制、重导入合并、异步生成取消、失败保留上一产品与模块生命周期。
- 增加独立物理材质资产的持久引用及 world 内解析，区分共享几何、实例参数与临时模拟状态。
- 设计物理 streaming 的异步准备、就绪检查、安全点激活/卸载、共享缓存与预算；一个游戏 World 的分块进入同一物理 world。
- 明确 World 分块清单、稳定实例身份、持久动态对象和打包依赖的宿主契约；World 分区调度属于 Framework 的配套设计，不在 Jolt 内另建分区系统。
- 保留现有 inline ShapeDesc 序列化，逐步新增资产引用。2026-10-27 按用户确认实施物理材质、Collision 产品及 Framework cell 加载闭环；完整制作窗口、自动拟合和 CoACD 保持后续任务。

## Capabilities

### New Capabilities

- `physics-collider-authoring`: 碰撞资产、生成工具、编辑器接入、cook 与验证的完整制作契约。
- `physics-streaming`: 资产准备、物理激活、驻留引用、取消与分块宿主的交互契约。

### Modified Capabilities

主 spec 已包含此前归档的 Jolt 基线；本 change 的增量规范覆盖当前已交付切片与后续目标，交付状态按 remaining-work.md 区分。Framework 显式 cell 格式已接通，空间调度和 Terrain collision 接线仍需各自验证，不将现有地形分页描述成已具备完整物理就绪保障。

## Impact

- 前置：`replace-bullet-with-jolt` 的接口与 adaptor + render 运行时结构。
- 拟扩展 `engine/physics` 的形状旋转、资产消费接口及关联数据；实现与资产 schema 留在 `plugins/jolt`。
- 拟新增 `plugins/jolt/editor`、`plugins/jolt/builder`，不新增 `core`。离线几何产品不依赖 runtime/adaptor；元数据 owner 与产品解析详见 design。
- 复用 Sandbox EditorCore / EditorUi / EditorWidgets、资产产品与构建任务接口、Aurora CPU 几何与预览接口。
- CoACD 为后续仅工具侧的候选依赖，尚未加入三方清单；需要固定 revision、异常路径和质量预算验证。
- 用户已确认继续实施：新增 JoltPhysics.Builder，并接入编辑器配置和 AssetTool；保留 adaptor + render，不创建 core。编辑器专用制作模块尚未新增。
