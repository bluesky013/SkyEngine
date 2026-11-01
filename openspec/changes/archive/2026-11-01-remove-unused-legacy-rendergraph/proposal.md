> 2026-11-01 整理：**ready-to-archive**。现行 owner：旧图退役记录；现行语义由后续 FrameGraph 契约维护。
> 剩余：旧 macOS 门已有后继原生证据补齐；25 条 delta 与现主 spec 全文一致，申请 history-only 归档。下方较早状态与验收记录保留为历史。

## Why

The old RenderGraph executor no longer has production callers. Its only consumers are legacy PipelinePass templates and dedicated tests, while renderer hosts use FrameComposer, FrozenFrameGraph and DeviceFrameContext. Keeping both APIs leaves misleading resource-binding and lifecycle contracts and a second backend factory to maintain.

## What Changes

- **BREAKING**: remove RenderGraph/CompiledGraph, old builders/handles/context, the immediate transient pool, and RDGBackend/CreateRDGBackend in all three backends.
- Remove the unused PipelinePass class hierarchy, GlobalRenderResources, BatchAllocator and BatchPackWriter, plus tests that only exercise them.
- Preserve shared Pass shader declarations, target reflection, frame features, FrameTransientPool, direct RHI and viewport APIs, and independent portions of mixed tests.
- Reconcile main specs, active changes, Aurora rules and current authoring docs; retain dated/archive history as history.

## Impact

Only existing Aurora targets change; no new target, module ownership, load entry or directory move. The public Device vtable changes, so all locally built backends and hosts must be rebuilt together. There is no supported old-API production caller to migrate in this repository; external users of those public headers must adopt frame features or direct RHI.

Implementation, this change and the affected main specs belong in the same eventual commit. Commit and OpenSpec archive still require user confirmation.
