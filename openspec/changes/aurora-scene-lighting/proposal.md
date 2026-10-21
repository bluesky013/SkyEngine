## Why

World objects now reach the shared renderer, but the default scene pass only applies a tint. The approved next stages require scene lighting, shadow resources and environment inputs to use the same pass-owned shared declarations as ordinary draws.

## What Changes

- Add validated depth sampling and depth-only raster attachments, followed by HDR color targets, to the existing frozen graph and backend compilers.
- Add a bounded forward-lighting scene pass and a directional shadow pass, including casters outside the camera frustum.
- Add explicit environment input bindings, diffuse/specular IBL and a final tone mapping pass with neutral defaults when inputs are absent.
- Preserve Global set 0 / Pass set 1 (View optional) / Batch set 2. Pass declarations are shared `.h` sources consumed directly by C++ and Slang; CMake only embeds source text.
- Verify graph rejection paths and actual pixels independently in Vulkan dynamic rendering, Vulkan render passes and DX12. Metal implementation and unexecuted native validation are recorded separately.

## Capabilities

### New Capabilities
- `aurora-scene-lighting`: Pass-owned light, shadow and environment inputs with explicit defaults, bounded work and native pixel verification.

### Modified Capabilities

None. This capability specifies the additive scene pipeline and its required frozen-graph subset.

## Impact

Existing Aurora pipeline, core preparation, adaptor components, shader generation and RHI compilers are affected. There are no new module targets, ownership changes or directory moves. PreparedRaster retains only geometry/exact-version resolution; the editor still has one packet producer and event consumer. Implementation and this change are reviewed together; archive and commit require user confirmation.
