> 2026-11-01 整理：**ready-to-archive**。现行 owner：World 编辑闭环里程碑；共享 bridge 的静态分析债务仍由 `aurora-scene-bridge` 5.3 跟踪。
> 剩余：9/9 tasks 已有自动化和原生 UI 证据，无独立实现余项。下方较早状态与验收记录保留为历史。

## Why

World extraction needs an editor authoring loop: create an object, edit its reflected component values, see the viewport update, and save/reopen the same logical scene. The current outliner shows only selection records and the property source contains no World objects.

## What Changes

- Connect the existing editor renderer's sole packet producer to `RenderSceneBridge` after the active simulation tick.
- Add scene object actions through `EditorActionRegistry`, owned undo commands, a World-backed outliner/property source, and viewport extent updates.
- Expose the concrete component's reflected object address through the existing component runtime-info macro, without RTTI downcasts.
- Cover create/edit/undo/delete/save/reopen and repair const-getter undo capture discovered by this loop.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `editor-viewport`: editable logical World contents feed the renderer through the owned bridge protocol.

## Impact

Existing Framework, Aurora Adaptor/Pipeline, Sandbox Module/Render/Shell/Core targets. No new target, module entry point, plugin dependency or directory relocation. Component virtual interface changes require rebuilding consuming modules together.
