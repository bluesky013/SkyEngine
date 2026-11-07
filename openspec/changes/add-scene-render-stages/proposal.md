## Why

Transparent scene features such as particles must render after opaque geometry and before tonemap, sharing the scene color and depth attachments. `SceneRenderFeature` currently owns opaque, HDR allocation, tonemap and selection in one feature, so no other feature can insert work at that point or read the scene depth.

## What Changes

- Add an ordered scene render stage extension contract to the production scene feature.
- Let registered extensions declare their own graph work after the scene opaque pass and before tonemap.
- Provide the extension with the current view, output color handle and scene depth handle for the duration of the callback.
- Keep extension registration, ownership and retirement coordinator-owned.
- Preserve existing behavior when no extension is registered.
- No new targets, modules or third-party dependencies.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `aurora-pipeline`: The production scene feature exposes an ordered post-opaque/pre-tonemap extension stage with shared scene color and depth.
- `aurora-renderer`: Scene extension registration and retirement are coordinator-owned commands with explicit lifetime and no retained borrowed context.

## Impact

- `engine/aurora/core/include/aurora/renderer/IRenderFeature.h`, render commands and feature registry.
- `engine/aurora/pipeline/src/SceneRenderFeature.cpp` and pipeline tests.
- Enables `add-particle-system`; takes no dependency on particle code.
