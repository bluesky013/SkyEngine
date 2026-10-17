## Why

Aurora has replaced the pre-Aurora renderer, shader pipeline, and Qt editor, but the legacy source trees and their build switches still ship: roughly 690 files under `engine/render`, `engine/shader`, and `engine/editor` remain buildable behind `SKY_BUILD_LEGACY_RENDER` / `SKY_BUILD_EDITOR`. Their remaining consumers (`plugins/xr`, PVS render glue, `tools/noise`, `tools/perf`) keep otherwise-dead targets and third-party dependencies alive, while the default build no longer uses any of them. This is now pure maintenance cost and a source of stale documentation and dependency declarations.

## What Changes

- Delete the legacy render stack (`engine/render/**`), legacy shader compiler (`engine/shader/**`), and legacy Qt editor (`engine/editor/**`).
- Remove `SKY_BUILD_LEGACY_RENDER` and `SKY_BUILD_EDITOR`; the sandbox editor (`SKY_BUILD_SANDBOX`) becomes the only editor build. Remove the related engine/top-level/plugin gating.
- Delete legacy consumers:
  - `plugins/xr` (its only RHI is the legacy `render/RHI`);
  - the PVS legacy-render glue (`plugins/pvs/runtime/render/**`, `PVS.Render`, `PVS` module registration);
  - `tools/noise` and `tools/perf` (legacy `RenderAdaptor` / `VulkanRHI` only, already excluded from the build).
- Remove legacy-only third-party dependencies and their find modules: `boost`, `taskflow`, `glslang`, `SPIRV-Cross`, `OpenXR`, `gles`, `implot`. Keep `GKlib`/`metis` (and their patches) reserved for future meshlet work.
- Remove unused find modules with no consumer: `ktx`, `mimalloc`, `cxxopts`.
- Update current build docs, plugin docs, and architecture/skill references to describe Aurora + sandbox only; archived OpenSpec records stay historical.
- **BREAKING**: `-DSKY_BUILD_LEGACY_RENDER=ON` and `-DSKY_BUILD_EDITOR=ON` stop being accepted; Qt5 is no longer required; the `XRModule` plugin no longer exists; PVS no longer provides a render/visualizer module.

## Capabilities

### New Capabilities

- `legacy-stack-retirement`: the repository builds and documents only the Aurora renderer, Aurora shader pipeline, and sandbox editor, with no legacy source trees, flags, consumers, or legacy-only third-party dependencies.

### Modified Capabilities

None. Aurora behavior, sandbox editor behavior, and PVS core culling are unchanged.

## Impact

- Deletes ~690 legacy engine files, the disabled XR plugin, PVS render glue, and two unused tool source trees.
- Build: removes two options and their CMake branches; default desktop/mobile builds no longer probe Qt5 or legacy third-party packages.
- Third-party: removes nine legacy-only dependencies from CMake discovery and two packages from `thirdparty.json`; no new dependencies.
- Docs/skills: current build and architecture references updated; historical archive records untouched.
- No runtime behavior change for Aurora or the sandbox editor.
