## Why

World extraction needs an editor authoring loop: create an object, edit its reflected component values, see the viewport update, and save/reopen the same logical scene. The current outliner shows only selection records and the property source contains no World objects.

## What Changes

- Connect the existing editor renderer's sole packet producer to `RenderSceneBridge` after the active simulation tick.
- Add scene object actions through `EditorActionRegistry`, owned undo commands, a World-backed outliner/property source, and viewport extent updates.
- Expose the concrete component's reflected object address through the existing component runtime-info macro, without RTTI downcasts.
- Cover create/edit/undo/delete/save/reopen and repair const-getter undo capture discovered by this loop.

## Capabilities

### New Capabilities
- `editor-world-rendering`: editable logical World contents feed the renderer through the owned bridge protocol.

### Modified Capabilities

## Impact

Existing Framework, Aurora Adaptor/Pipeline, Sandbox Module/Render/Shell/Core targets. No new target, module entry point, plugin dependency or directory relocation. Component virtual interface changes require rebuilding consuming modules together.
