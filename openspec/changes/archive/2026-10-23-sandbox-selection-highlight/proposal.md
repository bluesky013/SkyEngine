## Why

Viewport picking updates the editor selection, but the scene gives little visual feedback beyond the transform gizmo. Selected geometry needs a recognizable silhouette and non-mesh objects need a visible fallback, including selections made in the outliner.

## What Changes

- Add an editor-driven orange selection outline, with subdued feedback behind occluders and theme-controlled width/colors.
- Reuse the scene depth and redraw only selected supported geometry into a mask; empty selection adds no rendering passes.
- Keep non-mesh/custom geometry feedback through projected bounds or a pivot marker, and highlight selected overlay proxies.
- Keep selection ownership, document lifecycle and play-mode policy in Editor; provide only generic highlight rendering in the existing Aurora Pipeline.
- Verify selection changes, occlusion, clearing, document changes and native Vulkan/DX12 rendering.

## Capabilities

### New Capabilities
- `editor-selection-highlight`: Editor selection feedback and its rendering/lifecycle contract.

### Modified Capabilities
None.

## Impact

Existing EditorCore/EditorRender/EditorShell/SandboxModule and Aurora Pipeline/Adaptor targets; shared shader declarations and resources. No new modules, target ownership changes, runtime selection service or asset material mutations. Depends on the current uncommitted `sandbox-viewport-picking` implementation; commit/archive remain separate confirmed actions.
