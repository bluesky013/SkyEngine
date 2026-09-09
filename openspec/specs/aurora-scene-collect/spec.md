# aurora-scene-collect Specification

## Purpose
TBD - created by archiving change aurora-scene-collect. Update Purpose after archive.
## Requirements
### Requirement: SceneView（aurora）

`SceneView` SHALL 提供 frustum（6 plane）+ view/viewProject 矩阵，以及 `FrustumCulling(const AABB&) -> bool`。v1 只为 culling 与排序服务，不涉及常量上传。

#### Scenario: frustum 剔除
- **WHEN** primitive 的 worldBounds 完全在 view frustum 外
- **THEN** `FrustumCulling(bounds)` 返回 false，该 primitive 被剔除

### Requirement: RenderPrimitive（aurora）

`RenderPrimitive` SHALL 持有 geometry（vb/ib/offsets/draw args）+ worldBounds + 按 technique tag 索引的 `TechniqueBinding`（`pso` + `batchResourceGroup`）映射，并提供 `GatherRenderItem(context)`：context.tag 命中持有的 tag 才 append `DrawItem`；context.tag 为空时不过滤（append 默认 binding）。

primitive 为持久对象，其内部容器 SHALL NOT 绑定帧 arena。

#### Scenario: tag 过滤收集
- **WHEN** primitive 持有 "opaque" 与 "shadow" 两份 binding，收集 context.tag = "opaque"
- **THEN** 只 append opaque binding 对应的 DrawItem；shadow binding 不被收集

#### Scenario: 空 tag 不过滤
- **WHEN** 收集 context.tag 为空
- **THEN** primitive append 其默认 binding（保持默认 queue 兼容）

### Requirement: RenderScene（aurora）

`RenderScene` SHALL 管理 primitives 注册/注销与 views 集合。v1 无遮挡剔除系统。

#### Scenario: primitive 注册
- **WHEN** `scene.AddPrimitive(p)` 后 `scene.GetPrimitives()`
- **THEN** p 出现在 primitives 列表中

### Requirement: pass 收集链路

`SceneRasterPassTemplate` SHALL 提供 `SetScene(RenderScene*)` / `SetView(SceneView*)`，`Collect(builder)` 默认实现 SHALL：对 pass 内每个 queue，用 queue.techniqueTag 作过滤、`view.FrustumCulling` 做剔除，遍历 scene primitives 收集 DrawItem 到对应 queue，收集后按 queue.sortPolicy 排序（`FRONT_TO_BACK` = view 空间深度升序，`BACK_TO_FRONT` 降序，`NONE` 不排）。

收集 SHALL 发生在 BuildRDG（setup 期，Compile 之前）；RDG 层不感知 scene。

#### Scenario: OpaquePass 收集 opaque draw call
- **WHEN** scene 中有持 "opaque" tag 的 primitives，OpaquePass（main view，queue tag "opaque"）BuildRDG
- **THEN** "opaque" queue 的 items 恰好包含 frustum 内所有 opaque primitives，按深度升序

#### Scenario: 不同 queue 不同 tag 互不污染
- **WHEN** 同一 pass 有 tag="opaque" 与 tag="shadow" 两个 queue
- **THEN** opaque queue 只含 opaque primitives，shadow queue 只含 shadow primitives

