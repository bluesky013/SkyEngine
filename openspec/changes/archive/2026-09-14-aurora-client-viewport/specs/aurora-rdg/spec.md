## ADDED Requirements

### Requirement: BindViewport 资源

`RenderGraph` SHALL 提供 `BindViewport(const Name &name, RenderViewport *viewport) -> RDGTextureHandle`，声明 viewport 的 backbuffer 为图资源（`ResourceTag` 新增 `ViewportImageTag`，存 `RenderViewport*`）。

`ViewportImageTag` 资源 SHALL 是 culling 种子（其 lastWriterPass 恒 live）。prepare 阶段（`BindTransientResources`）SHALL 调 `viewport->Acquire()`：成功则 `mResolvedImages[i] = viewport->GetBackbuffer()`（裸 `Image*`，不 refcount）；失败则该资源 unresolved，写它的 pass SHALL 被 cull。

#### Scenario: BindViewport 返回可用 handle

- **WHEN** `auto bb = graph->BindViewport(Name("backbuffer"), viewport)` 且 `viewport->Acquire()` 成功
- **THEN** `bb` 有效；后续 pass 可对 `bb` 做 ColorAttachment / `AddPresentPass(SetSource(bb))`

#### Scenario: backbuffer 是 culling 种子

- **WHEN** backbuffer 经 `BindViewport` 声明且其 writer pass 无其它 consumer
- **THEN** 该 writer pass 仍为 live（不被剔除）

#### Scenario: Acquire 失败则写者 pass 被 cull

- **WHEN** prepare 阶段 `viewport->Acquire()` 返回 false
- **THEN** 写该 backbuffer 的 pass 不 emit（被 cull），后续 `Release()` 变 no-op
