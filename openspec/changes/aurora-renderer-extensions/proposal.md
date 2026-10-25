> 2026-10-26：旧 RenderGraph 及其 PipelinePass 模板已退役；相关旧模板目标不再实施，参见 `remove-unused-legacy-rendergraph`。其余目标维持原验收状态。

## Why

Status: debt-record（未排期）。当前 Renderer 已完成 Windows RGBA8/D32 最小绘制与 UI；动态 CPU prepare 注册、旧 pipeline 模板迁移和 HDR 是尚未交付的目标设计。归档主 change 时必须保留这些待办，不能把目标条款同步成已完成行为。

## What Changes

- 接收原 3.11 的 CPU prepare 扩展和 4.1 的旧模板/显式 ViewRenderContext 迁移。
- 保留原 pipeline 目标 delta，实施时按实际抽象复审；不要求沿用旧类名或复制一套管线。
- 记录更多光类型、完整材质/HDR、Compute/Custom、storage/跨队列的扩展条件；每类开始前须限定格式、能力与可执行验收，不默认本 change 一次实现全部。

## Capabilities

### Modified Capabilities

- `aurora-pipeline`: 旧 pass 模板与显式逐 View 管线/HDR 迁移的待实施契约。

## Impact

沿用 Aurora core/adaptor/runtime/rhi；World 资产事件输入由 aurora-scene-bridge 提供，CPU prepare 与 frame feature 保持各自职责。不得为支持 CPU/GPU 同构强行统一 system 与 pass。Metal 平台对等属于 aurora-renderer-platform-parity。
