## Why

GPU-driven features need simulation state that survives frames and draw/dispatch counts produced on the GPU. The current feature API only exposes transient creation and per-frame upload, and declarative raster/compute passes have no indirect form, which forces features onto Custom passes or CPU readback.

## What Changes

- Expose checked persistent buffer creation to `FrameFeaturePrepareContext` and external buffer import to the feature build context.
- Imported buffers remain caller-owned and reference-counted; the graph only retains them until submission completion.
- Add optional indirect raster draw and indirect compute dispatch declarations.
- Gate indirect declarations on actual device features and return an explicit Unsupported error when unavailable.
- Extend resource-use analysis, validation, content dependency, lifetime and native lowering for imported buffers and indirect arguments.
- No new targets, modules or third-party dependencies.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `aurora-framegraph`: Feature-visible external buffer import plus indirect raster draw and indirect compute dispatch declarations, including validation, lifetime, synchronization and native encoding.
- `aurora-renderer`: Feature prepare/build services expose persistent buffer creation and external import without exposing device submission or mutable device access.

## Impact

- `engine/aurora/core/include/aurora/renderer/IRenderFeature.h`, `FrameComposer`, `FrameGraphBuilder`, graph analysis and tests.
- `engine/aurora/rhi/interface/include/aurora/rdg/FrozenFrameGraph.h`, frame binding/validation code and Vulkan/DX12/Metal compilers.
- Enables `add-particle-system` and later GPU-driven terrain/vegetation work; it does not depend on those changes.
