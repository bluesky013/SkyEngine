## Context

`plugins/pvs` implements precomputed visibility (PVS): an offline bake rendered object ids from many sample points
inside editor-authored volumes, read the id buffer back, and packed per-cell visibility bitsets into per-sector
files. At runtime `PVSLoader` streams sectors around the view, `PVSCulling` implements `IRenderSceneCulling` and
answers per-object visibility, and `PVSVisualizer` draws debug geometry. All of it was built on the legacy render
stack (`RenderCore`) and, for the bake, the legacy Qt editor (`EditorFramework`, `Editor`).

Review findings on `dev_refactor_rhi`:

- **Build / hygiene**
  - `PVSWorldBuilder.cpp` (editor): unbalanced braces — 36 `{` vs 37 `}` — and a plugin-escaping relative include
    (`<../../../../engine/render/core/include/render/IStaticRenderObject.h>`). Both survived only because
    `PVS.Editor` is skipped when `SKY_BUILD_EDITOR=OFF`.
  - `PVSCulling.cpp:15`: stray line-continuation backslash.
  - `PVSVolume.cpp:24`: doubled `;;`.
  - `PVSDrawIDPass` ctor assigned `image.format = colorFormat` then immediately overwrote it with `depthStenFormat`.
- **Legacy render / editor coupling** (blocking the RHI refactor)
  - Runtime: `PVSCulling : IRenderSceneCulling` (`render/RenderScene.h`), `PVSModule : IRenderSystemEvent`
    (`render/Renderer.h`), `PVSVisualizer` (`render/debug/VolumeRenderer.h`); links `RenderCore`.
  - Editor: `PVSEditorModule` / `PVSVolume` / `PVSBakePipeline` used legacy RDG (`RasterPass`, `RenderGraph`),
    `EditorFramework`, `EditorActorCreation`, `Renderer::Get()->RegisterRenderFeature`, `Editor` target.
- **Ownership / lifetime**
  - `PVSCulling::PrepareCullingViewData` returns `new PVSCullingViewData` per view; that is correct — the result is
    adopted by the render scene's `CounterPtr<RenderSceneCullingViewData>` (verified in `RenderSceneVisitor.cpp`),
    so this is not a leak. The real defect is the missing null guard.
  - `PrepareCullingViewData` dereferences `loader` without a null check (crash if `Init` failed / not active).
  - `QueryVisible` reads `pvsData->data[...]` with no null or size guard.
- **Streaming / query semantics**
  - `PVSLoader::Update` loads sectors synchronously on the caller (main) thread; a sector-cache miss at a sector
    boundary causes a frame hitch. *(still open — async loading is a follow-up)*
  - `QueryVisibility` only consulted `currentSector`, so a query for a cell in a streamed neighbor sector returned
    `nullptr` and silently disabled culling for that view. *(fixed: the query now resolves the sector containing
    the cell)*
  - A missing sector load was silently dropped. *(fixed: recorded via the missing-sector counter without failing
    the update)*
- **Wiring / tests**
  - PVS is absent from `configs/modules_game.json` and `configs/modules_editor.json` → it never loads in the new
    launcher/editor path.
  - `PVSTest` linked `PVS.Static` (→ `RenderCore`) but only exercised `PVSConfig` grid math.

## Goals / Non-Goals

**Goals:**
- Split the runtime into a render/editor-free core (`PVS.Core`) and isolated legacy-render glue (`PVS.Render`), so
  the grid/streaming/query logic builds and tests independently of the legacy stack.
- Remove the legacy editor adaptation (`plugins/pvs/editor`, `PVS.Editor`), which is also what contained the
  latent compile break and the plugin-escaping include.
- Define the `pvs-culling` contract (grid/config math, streaming hysteresis, query semantics) in a spec.
- Fix concrete correctness / lifetime defects so the core is safe to reason about and to eventually re-integrate.
- Make tests exercise the loader/query logic, not only grid math, without dragging in `RenderCore`.

**Non-Goals:**
- Migrating the runtime culling glue to Aurora RHI — the core is decoupled here, but re-integrating it with Aurora
  is out of scope (the editor bake pipeline is removed, not migrated).
- Re-adding PVS to the runtime module configs.
- Porting the PVS bake to the non-Qt sandbox editor.

## Decisions

- **D1 — Keep the core, remove the legacy editor adaptation, isolate the legacy-render glue.** The bake pipeline
  and editor component were deeply tied to the legacy RDG and the legacy editor; rather than port them, delete
  them and keep the render-independent core (`PVS.Core`) plus the isolated runtime glue (`PVS.Render`). Rationale:
  the RHI refactor is migrating away from the legacy stack; a big-bang bake port would not land cleanly.
  Alternative considered: keep the editor adaptation behind `SKY_BUILD_EDITOR` — rejected, because it was already
  broken when built and pulls the legacy editor forward with no owner.
- **D2 — New `pvs-culling` capability spec.** PVS had no spec, so "correct" was undefined. Encode the
  data-layout/config math, streaming hysteresis, and query contract so fixes and tests have a target.
- **D3 — Conservative query fallback.** When a cell's visibility data is unavailable (sector not loaded, query
  outside the loaded set, or stale `currentSector`), `QueryVisible` SHALL return visible (do not cull) rather than
  crash or silently cull. Rationale: correctness over performance; unloaded data must never hide geometry.
- **D4 — Keep PVS out of the runtime module configs** until it is re-integrated on Aurora. Rationale: the runtime
  glue is legacy-render dependent; enabling it would load `RenderCore` paths on the Aurora runtime.
- **D5 — The core owns the on-disk format in both directions.** Alongside the read provider (`IPVSSectorProvider`)
  add the symmetric write sink (`IPVSSectorWriter` / `PVSSectorWriter`), so a future bake or external tool can
  serialize PVS data without the removed editor pipeline. Alternative considered: leave writing to callers —
  rejected, because then every writer re-implements the format and can drift from the reader.

## Risks / Trade-offs

- [Removing the editor bake loses the PVS authoring path until a replacement exists] → the core + spec are kept so
  a future Aurora bake/consumer can reuse them; the change is small and local to `plugins/pvs`.
- [Conservative fallback (D3) reduces PVS effectiveness when sectors are missing] → keep the streaming radius
  sufficient; correctness is still preferred.
- [Tests without `RenderCore` cannot link `PVSCulling` (which needs `IRenderSceneCulling`)] → resolved by the
  target split: tests link `PVS.Core` only, and the query/streaming logic lives in the core.
- [The runtime `PVS` glue is now inert (nothing calls `PVSCulling::Init` after the editor removal)] → it is left
  isolated and unloaded; reviving it is the follow-up migration change.

## Migration Plan

1. Split the runtime into `PVS.Core` (render/editor-free) and `PVS.Render` (legacy glue); re-point `PVS` at
   `PVS.Render`. *(done)*
2. Delete `plugins/pvs/editor/` and the `PVS.Editor` target; drop it from `plugin.json`. *(done)*
3. Fix hygiene + null-safety; resolve the query's sector from the queried cell; record missing sector loads; add
   the serialization write sink. Build `PVS.Core`, `PVS.Render`, `PVS` and run `PVSTest` on the core. *(done)*
4. Leave module configs unchanged (PVS stays unloaded until the follow-up migration).
5. Rollback: the changes are local to `plugins/pvs` and the new spec; reverting the commit restores prior state.

## Open Questions

- Should `IRenderSceneCulling`/`RenderSceneCullingViewData` be re-homed so the runtime PVS can drop `RenderCore`
  before any Aurora re-integration?
- Is a second, non-render consumer of the visibility bitset expected (e.g. streaming/audio), which would push the
  loader/query into a render-independent target now? (Already true after this change.)
