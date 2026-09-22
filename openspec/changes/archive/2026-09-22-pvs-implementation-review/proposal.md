## Why

The `pvs` plugin (`plugins/pvs`) is fully coupled to the **legacy render stack** (`RenderCore`:
`render/RenderScene.h`, `render/Renderer.h`, `render/debug/VolumeRenderer.h`, legacy RDG `RasterPass`) and to the
legacy Qt editor (`EditorFramework`, `EditorActorCreation`, `Editor`). On the `dev_refactor_rhi` branch the runtime
and editor are moving to Aurora / the non-Qt sandbox, and PVS is now **orphaned**: it is not listed in
`configs/modules_game.json` or `configs/modules_editor.json`, so `PVS` never loads, and the editor-side
adaptation (`PVS.Editor`, legacy Qt editor) only built when `SKY_BUILD_EDITOR=ON`. A read-through also surfaced
concrete defects hidden by that (the editor target is not built on this branch), plus contract and ownership gaps.
This change captures the review and lands the remediation: keep the render-independent core, drop the legacy
editor adaptation, and isolate the legacy-render glue.

## What Changes

- **Decouple the core from the legacy render / editor stack**: split `plugins/pvs/runtime` into `runtime/core`
  (`PVS.Core`, linked against `Core`/`Framework` only) and `runtime/render` (`PVS.Render`, the legacy
  `IRenderSceneCulling` glue); the shared `PVS` target links `PVS.Render`. Extract the render-independent
  visibility primitives into `PVSVisibility`, make the sector source abstract (`IPVSSectorProvider`) so the loader
  can be driven in memory, and add the symmetric write sink (`IPVSSectorWriter` / `PVSSectorWriter`) so the core
  owns both read and write of the PVS on-disk format. The core and its tests no longer depend on `RenderCore` or
  the editor.
- **Drop the legacy editor adaptation**: delete `plugins/pvs/editor/` (bake pipeline, `PVSVolume`,
  `PVSEditorModule`) and the `PVS.Editor` target, and remove it from `plugins/pvs/plugin.json`. This also removes
  the latent unbalanced-brace compile break and the plugin-escaping relative include in `PVSWorldBuilder.cpp`.
- **Document the PVS contract**: introduce a `pvs-culling` capability spec for the PVS data layout/config, sector
  streaming (hysteresis), and the visibility query, which is currently undocumented.
- **Fix ownership / lifetime defects**: document the ref-counted per-view `RenderSceneCullingViewData` (adopted by
  the scene's `CounterPtr`, not a leak), fix per-build `Buffer` / `ResourceGroup` growth without reset, and guard
  the `loader` dereference in `PrepareCullingViewData` and the `data` dereference in `QueryVisible`.
- **Harden streaming and query**: resolve the queried cell's own sector so cells in loaded neighbor sectors are
  answered (instead of only the main-view `currentSector`), and record missing-sector loads without failing the
  streaming update. (Asynchronous sector loading remains a follow-up.)
- **Codify hygiene/tests**: cover the loader/query (not only `PVSConfig` math) and stop linking the whole legacy
  `RenderCore` into a test target that only exercises grid math.
- **State the migration decision**: PVS SHALL NOT be re-added to the runtime module configs until it is migrated
  off the legacy render stack. The **core is render/editor-free** and the **legacy editor adaptation is removed**;
  reviving the runtime culling integration on Aurora is a **follow-up** change.

## Capabilities

### New Capabilities
- `pvs-culling`: the PVS precomputed-visibility contract — grid/config math, sector streaming with load/unload
  hysteresis, and per-object visibility queries — plus the requirement that the plugin build and its tests do not
  depend on the legacy render stack beyond what the runtime culling interface requires.

### Modified Capabilities
<!-- None: no existing spec covers PVS. -->

## Impact

- `plugins/pvs/runtime/core/**` + `plugins/pvs/runtime/render/**` (new split; `PVSTypes.h`, `PVSLoader.*`,
  `PVSVisibility.*` in core; `PVSCulling.*`, `PVSModule.*`, `PVSVisualizer.*` in render).
- `plugins/pvs/editor/**` removed (bake pipeline, `PVSVolume`, `PVSEditorModule`, editor `Registry.cpp`).
- `plugins/pvs/CMakeLists.txt` (`PVS.Core` / `PVS.Render` targets, test linked against the core only) and
  `plugins/pvs/plugin.json` (`PVS.Editor` entry removed).
- `plugins/pvs/test/PVSTest.cpp` (visibility + streaming tests).
- New spec: `openspec/specs/pvs-culling/spec.md` (via this change's delta).
- Not changed here: reviving the runtime culling integration on Aurora (follow-up change).
