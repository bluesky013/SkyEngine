## Context

The Aurora renderer and shader compiler plus the non-Qt sandbox editor are the active runtime and editor stacks. The legacy trees still exist for opt-in builds:

- `engine/render` (631 files): `RenderCore`, `RenderAdaptor`, `RHI`, Vulkan/DX12/Metal/GLES backends, the legacy asset builder, and the Qt `SkyRender.Editor`.
- `engine/shader` (37 files): the pre-Aurora `ShaderCompiler`/tool/tests.
- `engine/editor` (59 files): the Qt editor and `EditorFramework`.

Consumers that survive only because of the legacy stack:

- `plugins/xr` links `RenderCore` and legacy `RHI`; it is disabled by default.
- `plugins/pvs/runtime/render` and the `PVS.Render`/`PVS` targets link `RenderCore`; `PVS.Core` is independent.
- `tools/noise` links `RenderAdaptor`; `tools/perf` links `VulkanRHI.Static`. Both are already excluded from `tools/CMakeLists.txt`.
- Legacy third-party discovery: `boost`, `taskflow`, `glslang`, `SPIRV-Cross`, `OpenXR`, `gles`, `implot`. `GKlib`/`metis` are legacy meshlet dependencies retained for future meshlet integration.

`plugins/xr` is not referenced by any active spec or runtime path. PVS documentation already declares the render/editor adaptation removed or isolated. Therefore deletion is the lowest-cost path; a future XR implementation would target Aurora RHI.

## Goals / Non-Goals

**Goals:**

- Remove all legacy render, shader, and Qt-editor code and build switches.
- Remove every target that exists only to serve them.
- Remove legacy-only third-party discovery and package declarations.
- Update live documentation so the build instructions and architecture describe only Aurora + sandbox.
- Keep Aurora, the sandbox editor, and PVS core behavior unchanged.

**Non-Goals:**

- No Aurora feature work, renderer migration, or shader conversion.
- No sandbox editor redesign.
- No deletion of legacy asset files (`.tech`, `.hlsl`) whose consumers may be audited separately; only code and its build wiring are in scope.
- No cleanup of ignored local build directories or prebuilt package caches under `build_3rd/`.
- No rewrite of archived OpenSpec records.

## Decisions

### 1. Reuse the existing debt change

`legacy-render-thirdparty-cleanup` already names this exact work and is referenced by the shader migration doc and consolidation notes. Replace its debt-record proposal with a scheduled change and add design/specs/tasks instead of creating a competing change.

### 2. Delete rather than keep a disabled flag

Keeping the trees behind an always-off flag preserves maintenance and dependency costs with no consumer. Removal is recoverable through git history. This also lets CMake stop probing Qt5 and legacy packages entirely.

### 3. Delete `plugins/xr`

`XRModule` implements `rhi::XRInterface` against the legacy RHI and has no Aurora counterpart. There is no active spec or build that enables it. Delete it and remove its `plugins.json` entry; future XR work must target Aurora.

Alternative considered: keep the plugin but disable compilation. Rejected because the sources would still reference deleted headers and remain a dead dependency trap.

### 4. Keep `PVS.Core`, delete `PVS.Render`

`PVS.Core` (grid/sector storage, streaming, fail-safe visibility) has no render dependency and is tested by `PVSTest`. Delete `plugins/pvs/runtime/render/**` and the `PVS.Render`/`PVS` module registration. Keep the plugin config for `PVS.Core`.

### 5. Delete `tools/noise` and `tools/perf`

Neither is in the active tools build. Both are pure legacy-render consumers; keeping them would leave dangling target names and stale asset commands.

### 6. Remove dependencies by evidence

Remove only dependencies with no remaining consumer:

| Dependency | Last consumer |
|---|---|
| `boost`, `taskflow` | legacy render/core, legacy shader |
| `glslang`, `SPIRV-Cross` | legacy shader |
| `OpenXR` | legacy RHI + XR plugin |
| `gles` | legacy GLES backend |
| `implot` | `tools/perf` |
| `ktx`, `mimalloc`, `cxxopts` | no consumer |

Keep dependencies still used by Aurora or the sandbox (`slang`, `dxcompiler`, `assimp`, `meshoptimizer`, `stb`, `nanosvg`, `freetype`, `astc`, `ispc_texcomp`, image/compression libraries, and test/network/audio packages).

### 7. Update live docs and skills only

Update `README.md`, `docs/plugins/pvs.md`, `docs/aurora/shader-design-and-migration.md`, current architecture/build skill copies, and any active-change notes that describe the legacy stack as pending. Archived OpenSpec records remain unchanged.

## Risks / Trade-offs

- **[Hidden runtime usage of legacy targets]** → Build the full project and run all tests after removal; grep for target names and legacy includes.
- **[XR functionality lost]** → No active spec or enabled build uses it; the change records that future XR must target Aurora.
- **[PVS visualizer removed]** → It is already isolated behind the legacy switch and has no caller in the default build; `PVS.Core` and its tests remain.
- **[Stale third-party cache causes a confusing configure]** → The ignored `build_3rd` cache may still contain paths, but CMake no longer asks for them; a clean configure validates the removed discovery.
- **[Docs/skill copies diverge]** → Update all live copies (`.opencode`, `.claude`, `.github`, `docs`, `README`) in the same change and leave archives untouched.
- **[Removing unused `ktx`/`mimalloc`/`cxxopts` find modules loses a future starting point]** → They have no package declaration or consumer; re-adding is trivial if needed.

## Migration Plan

1. Delete the legacy source trees and consumers.
2. Remove build options, gating, and legacy third-party discovery/manifest entries.
3. Update live docs and skills.
4. Reconfigure, build the full project, and run the test suite.
5. Commit code, specs, and archive state together.

Rollback is a single revert of the change; no runtime data or persisted format migration is involved.

## Open Questions

None.
