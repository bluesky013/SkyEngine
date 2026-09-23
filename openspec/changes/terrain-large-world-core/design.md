## Context

`plugins/terrain` currently compiles a single static library (`Terrain.Static`) that links `RenderAdaptor` and `Physics`, and whose headers include `render/resource/Buffer.h`, `render/RenderGeometry.h`, and `rhi/Device.h`. The "logic" side is therefore not separable from rendering:

- `TerrainComponent` holds POD data plus a raw `TerrainFeatureProcessor*`; it does not include render headers today, but the same static target does.
- `TerrainClipmap` computes LOD ring layout and immediately allocates `VertexBuffer`/`IndexBuffer` in the same class.
- `TerrainFeatureProcessor` (render layer) owns loading, tile cache, CPU queries and the renderer, so the only available query API lives behind a render feature.
- `TerrainData` stores `heightmapTiles`/`splatmapTiles` as flat `std::vector<Uuid>` with `tileCountX/Y` but no world-space addressing, no per-tile metadata, no streaming, and no per-world state.
- `QueryHeight`/`QueryNormal`/`QuerySplatWeights`/`Raycast` are stubs returning constants.
- Editor "Create Terrain" is commented out; `tools/` is dead duplicate code; `TerrainQuadTreeTest` names a class that no longer exists.

`engine/navigation` established the target shape: a render-agnostic core static library that links only `Framework`, exposes data/query interfaces, owns per-world runtime state as an `IWorldSubSystem`, pages tiles by focus with hysteresis, prefetches payloads off-tick, and hands render layers plain data. `plugins/recast` is the backend; render integration is a separate layer. Terrain has no third-party backend, but its render layer is exactly the analogue of `plugins/recast`, and its core is the analogue of `engine/navigation`.

Constraints: engine positions are 32-bit `float` `Vector3` (no world origin rebasing exists), the engine has no world-level streaming/partition system, and the active render target is aurora (`engine/aurora`), not the legacy `RenderAdaptor`.

## Goals / Non-Goals

**Goals:**

- Extract `engine/terrain` as a render-agnostic core (`Terrain` static, links `Framework` only) with no `render/` or `rhi/` include.
- Model terrain as a tiled heightfield with explicit world-space tile addressing and per-tile metadata.
- Provide a real CPU sampling/query API (`QueryHeight`, `QueryNormal`, `QuerySplatWeights`, `Raycast`) over loaded tile data.
- Provide focus-driven proximity tile streaming with load/unload hysteresis, per-tick apply budget, and off-tick async prefetch, with per-world isolation.
- Move all GPU work into an aurora render layer: feature processor, atlas construction, clipmap instanced rendering, material binding.
- Provide terrain collision by building per-tile static heightfield colliders from core height samples and streaming them with tiles, without introducing render dependencies.
- Rebuild editor authoring on the new core/component and reserve a sculpt/paint seam.
- Keep the dependency direction one-way: consumers -> `engine/terrain`; render consumers -> terrain render adaptor.

**Non-Goals:**

- World-level streaming/partition of actors (framework has none); terrain paging is subsystem-scoped like navigation.
- Origin rebasing, double-precision coordinates, or camera-relative rendering.
- Terrain sculpting/painting implementation (the editor only reserves the seam and restores generation/grid editing).
- Vegetation scattering, road/river/spline.
- Non-heightfield terrain collision authoring (per-tile heightfield colliders only) and any collision LOD.
- Implementing a GPU-driven (compute cull + indirect) terrain path; the aurora renderer may keep a traditional instanced draw.
- Node-graph biome authoring and GPU compute terrain generation; a fixed, configurable noise stack with a graph seam is enough for this change.
- UE5 import tooling (the format stays UE5-compatible; import is a separate change).

## Decisions

### D1: Layering -- `engine/terrain` core + `plugins/terrain` aurora render layer

**Decision**: Create `engine/terrain` (target `Terrain`, STATIC, `LINK_LIBS Framework`) holding the render-agnostic core, and reduce `plugins/terrain` to the aurora render adaptor plus module registration and editor. The core never includes `render/` or `rhi/`.

**Rationale**: Mirrors the proven `engine/navigation` + `plugins/recast` split; makes terrain usable by physics/vegetation/AI without pulling in rendering, and lets the render layer migrate independently. The user-selected target also keeps the door open to a future non-aurora render layer.

**Alternatives considered**: Splitting only inside `plugins/terrain` (`TerrainCore.Static` + adaptor) -- less churn but core stays plugin-scoped and unreusable engine-wide. Keeping one target and extracting interfaces only -- fails the "no render includes" constraint.

### D2: Tiled heightfield with world-space addressing

**Decision**: Model the terrain as a regular tile grid. Core types: `TerrainTileCoord {int32 x, y}`, `TerrainMeta` (tile vertex size, `resolution` meters/vertex, `heightFormat`, `heightScale`, `heightOffset`, `tileCountX/Y`, `origin`), `TerrainTileInfo` (coord, world bounds, min/max height). World<->tile conversion lives in core (`WorldToTile`, `TileToWorld`, `WorldToLocalTexel`) and is the single source of truth.

**Rationale**: Navigation and PVS both address space with integer grid coordinates derived from float positions; terrain must do the same to stream, to locate a query, and to let the editor pick a tile. Flat `Uuid` lists cannot express "which tile is here" without an implicit index convention.

**Alternatives considered**: Keep flat lists + index math (ambiguous at edges, no metadata for streaming/raycast early-out). Quadtree addressing (irregular, conflicts with the chosen clipmap LOD that is uniform per level).

### D3: Terrain asset format and component data model

**Decision**: Introduce a terrain asset that carries `TerrainMeta` + a tile manifest (`TerrainTileInfo[]`) + per-tile **multi-level LOD** heightmap payloads (each tile stores a LOD chain `LOD0..LODn`, halving the vertex resolution per level) plus splatmap payloads grouped 4 layers per RGBA tile (splat stored on the near/high-detail LODs only). Assets are produced by an offline builder (`engine/terrain/builder`, non-render, links `Framework` + `Terrain`) and can also be created in-editor. `TerrainComponent` holds only POD: `TerrainMeta`, terrain asset `Uuid` (or source `Uuid`), material `Uuid`, layer definitions, and streaming parameters. It resolves its runtime state through `TerrainSystem`, never a feature processor.

**Rationale**: Matches `NaviMeshData` (manifest + addressable tile payloads) and keeps the component logic-only. A per-tile LOD chain is what makes near/far rendering out-of-core: each clipmap ring loads the tile LOD it actually needs instead of full-resolution data everywhere.

**Alternatives considered**: Store all heights in one blob (defeats streaming/partial load). Keep storing raw texture `Uuid` lists (no addressing, no metadata, render-coupled to `Texture` assets). A single-resolution tile grid plus one resident coarse base layer (simpler, but caps far-field detail and world size; see D11).

### D4: Render-agnostic runtime core (`TerrainField` + `TerrainSystem`)

**Decision**: Core owns a runtime field object (tentatively `TerrainField`) holding loaded tile height/splat samples and the sampling queries, and a per-world `TerrainSystem : IWorldSubSystem` (name `"Terrain"`) owning the manifest, the field, streaming state, and tick/streaming updates. The component pushes config to `TerrainSystem` on `OnAttachToWorld`. Render layers consume core data through a plain-data handoff (`TerrainRenderData`: meta, visible tile rect, atlas source descriptors, clipmap level description) and a described interface seam, analogous to `NaviDebugGeometry`.

**Rationale**: This is the exact navigation pattern (`NavigationSystem` owns paging, `NaviMesh` owns query, render layers get plain data) and removes the feature processor from the logic path. It also gives a single attach point for physics/vegetation queries.

**Alternatives considered**: Keep `TerrainFeatureProcessor` as the query owner (forces render dependency onto consumers). Put field ownership on the component (no per-world shared paging, duplicated state).

### D5: Proximity streaming with hysteresis and off-tick prefetch

**Decision**: Copy the navigation streaming shape but make it **LOD-aware**: `availableTiles` manifest, a loaded/pending set keyed by `(tile coord, lod)`, a `focus` position, and per-LOD load/unload radii (annuli) with a hysteresis gap plus a per-tick apply budget. `UpdateStreaming()` runs each tick: unload tiles outside their LOD's unload radius, scan the annulus of each LOD and start an async `TerrainTileLoadTask` for missing payloads (copying persisted bytes off-tick), and apply finished loads on the main thread within budget; pending loads that drift out of range are cancelled. `OnTerrainChanged()` invalidates the loaded set. Tile+LOD state is per-`TerrainSystem` so worlds never share it.

**Rationale**: Proven design in `navigation-tile-streaming`; avoids re-deriving budgets, cancellation, and hysteresis semantics. Terrain payloads are larger than nav tiles, and out-of-core far-field requires loading coarser LODs farther out, so per-LOD radii/budgets are essential.

**Alternatives considered**: Synchronous load on focus change (hitches). No hysteresis (thrash at the radius boundary). Global/shared tile cache (breaks per-world isolation). Single LOD radius with one full-resolution payload per tile (memory-infeasible for far rings).

### D6: Query API in the core

**Decision**: `TerrainField` (or the field exposed by `TerrainSystem`) implements `QueryHeight(x, z)`, `QueryNormal(x, z)` (finite differences), `QuerySplatWeights(x, z)` (bilinear over the 4-layer splat tile), and `Raycast(ray, maxDist, hit)` (marched against loaded tiles, early-out by `TerrainTileInfo` bounds). Queries read only loaded tile data.

**Rationale**: Fulfills the archived `terrain-query` intent, but now from a render-free owner. Consumers (physics/vegetation/editor preview) depend on `engine/terrain` only.

**Alternatives considered**: Query from the render feature processor (render dependency for gameplay). Query by sampling GPU readback (latency, cost).

### D7: Aurora render layer

**Decision**: The aurora terrain render layer owns an aurora feature processor that each tick pulls `TerrainRenderData` from `TerrainSystem`, builds/updates **per-LOD** GPU heightmap+splatmap atlases from core tile payloads, and submits clipmap instanced draws through the aurora scene/RDG with the terrain technique/material. Clipmap geometry/LOD construction (currently in `TerrainClipmap`) moves here, since it is inherently GPU geometry, and handles LOD transitions (skirts/geomorph) between rings. The component is reflected/registered into the aurora adaptor group so the render layer can bridge actor -> feature processor without the core referencing render types.

**Rationale**: Satisfies "render decoupling" while targeting the active renderer. Keeps atlas construction and instancing where GPU state belongs, and keeps `engine/terrain` free of `rhi`.

**Alternatives considered**: Keep the legacy `RenderAdaptor`/`RenderPrimitive` path (contradicts the aurora migration). Keep GPU buffers in core behind `ITerrainRenderer` (still `rhi` includes in core).

### D8: Editor rebinding and reserved seam

**Decision**: Re-bind the editor tool/widgets to the new component and core: restore "Create Terrain" (create actor + `TerrainComponent` with `TerrainMeta`), make grid add/remove actually mutate the terrain asset/tile set, drive the generator from `TerrainGenerateConfig` (seed) and `TerrainMeta` (height format), and generate splatmaps. Overlay drawing moves behind an editor render helper that consumes core data. A documented no-op seam (interface + TODO) is reserved for sculpt/paint.

**Rationale**: The archived design flagged editor regression; authoring must produce the new asset shape or the pipeline is untestable end to end.

**Alternatives considered**: Leave editor for a follow-up (terrain asset cannot be created, blocking validation).

### D9: Retire dead code and re-focus tests

**Decision**: Delete `plugins/terrain/tools` (duplicate generator, orphan target, unused output) and the stale `TerrainQuadTreeTest` name; add core tests under `engine/terrain/test` covering tile addressing, asset serialization round-trip, `TerrainField` queries, and streaming paging (mirroring `NavigationTest`), plus a module load/factory test for the render module.

**Rationale**: Removes two sources of confusion and gives the refactor verifiable behavior.

### D10: Per-tile static heightfield collision

**Decision**: Consume the backend-neutral heightfield shape added by the `physics-backend-abstraction` change (that change owns `HeightFieldShape` and the Bullet `btHeightfieldTerrainShape` implementation). A terrain collision layer (links `Terrain` + `Physics`) builds one static `CollisionObject` per loaded tile from that tile's full-resolution height samples and `TerrainMeta` (resolution, height scale/offset, world origin), adds it to `PhysicsWorld` on tile load, and removes it on tile unload. Collision always uses full-resolution tile data and never follows the render clipmap LOD. The core exposes loaded tile samples/metadata to non-render consumers so the layer does not depend on the render layer.

**Rationale**: A heightfield is the natural, memory-light representation of a regular terrain grid and maps directly onto a Bullet shape that is already vendored. Per-tile colliders align exactly with the streaming tile lifecycle from D5, so collision paging is a byproduct of tile paging. Using the existing static `CollisionObject` (no rigid body) avoids dynamics overhead.

**Alternatives considered**: Per-tile triangle-mesh collider (works with the existing `btBvhTriangleMeshShape`, but two triangles per quad is far heavier, and the current path only accepts a cooked `Mesh` asset, so it would need a new runtime-mesh shape anyway). Query-only collision (no real shape; insufficient for `CharacterController` and rigid bodies that need collision geometry in the world). Whole-world collider (defeats the large-world memory/streaming goals).

### D11: Per-tile multi-level LOD (out-of-core) near/far data

**Decision**: Manage near/far mesh data with **per-tile LOD chains** rather than a single full-resolution grid. Every terrain tile stores a LOD chain (`lodCount` levels; `LOD0` = full `tileSize` quads, `LOD L` = `tileSize >> L` quads, down to a minimum). Streaming loads, per terrain LOD `q`, only the tiles inside `q`'s annulus (radii doubling like the clipmap: `radius(q) = baseRadius * 2^q`), keyed by `(coord, lod)`. The clipmap render layer maps each ring/level to the tile LOD the ring needs and samples that LOD's atlas; CPU queries/collision use `LOD0` (the highest detail) in the near disk. No quadtree: within each LOD the data is still a flat integer-addressed uniform grid; resolution is expressed as extra LOD payloads per tile, not as a hierarchical subdivision.

**Rationale**: This is the only scheme that keeps memory proportional to what is visible for large worlds (tens to hundreds of km^2): far rings load a few coarse samples, near rings load full detail. It also makes far-field visual quality uniform instead of limited by a fixed coarse base layer. Uniform addressing per LOD keeps lookups O(1) and avoids tree maintenance, while still being out-of-core.

**Alternatives considered**:
- **Single-resolution grid + resident coarse base layer** (the simpler scheme): far rings sample one always-resident coarse heightmap; near rings stream full-resolution tiles. Pros: far smaller effort, one LOD seam, predictable memory. Cons: far-field detail is capped by the base resolution, the base grows with world size, and there is a visible base<->fine transition band. Rejected because the target is a large open world where far-field detail and unbounded world size matter.
- **Quadtree / chunked-LOD** (variable-resolution tiles in a tree): more general, but conflicts with the uniform per-level clipmap LOD and the flat addressing/query model. Not needed once LOD chains are per-tile.
- **True virtual texturing / GPU sparse residency**: strongest scalability, but requires RHI sparse-resource support that aurora does not expose yet. Deferred.

### D12: Procedural terrain generation

**Decision**: Put a deterministic, render-free generation algorithm in `engine/terrain` and reduce the editor to config/preview/bake. Generation is per-tile and world-space continuous, so tiles are independent, order-free, and seamless:

1. **Height** from a configurable layered noise stack (fBm octaves with frequency/amplitude/lacunarity/gain, optional domain warp and ridged layers) sampled in world coordinates with the configured `seed`.
2. **Shaping**: optional height remap / terracing / erosion-like post-curve.
3. **Splatmap**: derive 4-layer RGBA weights from slope, height, and biome/layer rules.
4. **LOD chain**: build LOD0 at full tile resolution, then downsample to each coarser LOD (average for height, renormalized for splat) so coarse LODs stay consistent with LOD0.

Generation runs in two modes with identical output: offline cook in `engine/terrain/builder`, and on-demand generation when a tile+LOD is missing from the cooked asset (for infinite/streamed worlds). All of it runs off the main thread.

**Rationale**: Deterministic world-space sampling makes adjacent tiles match at borders without cross-tile dependencies, which is exactly what makes per-tile parallel baking and on-demand streaming generation possible. Keeping the algorithm in the core (not the editor) lets the offline builder, the runtime, and the editor share one implementation and keeps it testable and render-free. The old editor generator hardcodes the seed, samples with swapped axes, downsamples height with a max filter, and emits no splatmaps — none of which fit the per-tile LOD-chain asset.

**Alternatives considered**: GPU/shader-based generation (RHI-dependent, hard to bake/cache deterministically, no headless path). Editor-only generation (cannot feed offline cook or runtime streaming). Pre-made heightmaps only (no procedural path, but source assets remain supported).

### D13: LOD seam stitching

**Decision**: Stitch LOD seams in the render layer, never in the data. Data is already seam-free: continuous fields are sampled in world space and adjacent tiles share boundary samples (D12), so no data-level blending is needed. For geometry:

- **Skirt by default**: each clipmap block (and each rendered tile) extends a downward skirt along its edges, hiding the T-junction gap left by differing vertex spacing. Cheap, robust, and invisible in normal viewing.
- **Optional vertex geomorphing**: when enabled, coarse-level vertices blend between their own height and the position they would take at the finer neighbor's spacing, driven by the normalized distance within the level, which removes both the crack and the popping.
- **Same technique for tile-LOD adjacency**: when adjacent tiles render at different LODs (scheme B, D11), their shared edge uses the same stitch as clipmap ring boundaries.
- **Prefiltered transition/degenerate index variants** are the more general alternative but multiply GPU state combinations; deferred unless skirt/geomorph prove insufficient.

**Rationale**: Skirts are the lowest-risk first implementation and fully cover cracks; geomorphing additionally removes popping, which matters for a large open world. Keeping stitching render-only preserves the invariant that core data, CPU queries, and collision are LOD-independent (D6, `terrain-collision`).

**Alternatives considered**: Transition/degenerate index blocks (more combinations, more GPU state). Data-level blending across tile borders (would couple tiles and break the per-tile independence that makes parallel bake and on-demand generation possible).

### D14: GPU-driven terrain node (data/logic split)

**Decision**: The terrain render node SHALL support a GPU-driven path, with a strict split:

- **CPU (core logic)**: streaming/residency (which `(tile, LOD)` entries exist), atlas slot allocation, incremental GPU uploads, and CPU queries/collision. The core exposes meta, LOD description, tile bounds, and a **residency change delta** (added/removed `(coordinate, LOD)`) rather than only a snapshot.
- **GPU (driven logic)**: per-block/per-tile frustum culling, LOD/ring block selection, instance compaction, indirect draw argument generation, distance density, and wind/interaction.
- **Render data structures**: per-LOD height/splat atlases, a resident tile buffer (`coord, lod, atlasRect, bounds`), the clipmap block descriptor buffer, indirect args buffer, and camera/frustum uniforms.
- **Boundary**: the core hands plain data plus a delta; the render layer builds and updates GPU buffers; the CPU never iterates per-block or per-instance.
- A **CPU-driven path is retained as a fallback** (backends without compute/indirect, or for debugging).

**Rationale**: On large worlds, per-block/per-instance CPU work does not scale; moving selection/visibility/draw to the GPU keeps CPU cost flat while the residency split preserves the render-agnostic core and the one-way dependency. It also subsumes the D13 culling: with GPU-driven rendering, block/tile culling runs on the GPU instead of the CPU.

**Alternatives considered**: CPU-driven only (the base 7.x plan; correct but CPU-bound at scale). Fully pre-baked GPU data (breaks procedural generation, streaming, and per-world isolation).

**Risks / open items**: GPU buffer and atlas-slot lifecycle/defragmentation; correctness of the residency delta under streaming churn (per-frame coalescing); determinism of GPU culling order (stable ordering or accept visually-consistent non-determinism); stitching parameters (skirt/geomorph, D13) must be GPU-available; backends lacking compute/indirect need the fallback.

## Risks / Trade-offs

- [Large change surface] -> Split implementation into phases (core+data first, then streaming, then aurora render, then editor); each phase compiles and has tests.
- [Terrain payloads heavier than nav tiles] -> Keep off-tick prefetch + per-tick budget; LOD chains mean far tiles are coarse and cheap, but near full-resolution tiles are heavy, so measure a payload/LOD-aware budget.
- [Per-tile LOD cook and storage] -> A LOD chain multiplies stored data; cap `lodCount`, store splatmaps only on the near/high-detail LODs, and cook offline in `engine/terrain/builder`.
- [LOD transition popping and cracks] -> Ring and tile boundaries change vertex spacing, creating T-junction cracks and popping; stitch with skirts by default (D13) and optionally vertex geomorphing, and test visible transitions and steep-slope skirts.
- [LOD-aware streaming complexity] -> Loaded/pending state keyed by `(coord, lod)`, per-LOD annuli and budgets, and cancellation/replacement on LOD change; cover with tests for per-LOD loading, unload, and LOD switch.
- [A coordinate may need multiple LODs] -> Allow `(coord, lod)` entries to coexist during transitions; queries/collision always prefer `LOD0` when present.
- [Aurora terrain APIs not fully stable] -> Keep the core/render seam as plain data so the aurora layer can change without touching core; if aurora blocks, the core + tests still land.
- [Component/asset serialization break] -> Current terrain is non-functional (no loaded scene data expected); document the format and bump/define a new asset type.
- [Per-world paging scan cost is O(available) per tick] -> Accept for now (same as navigation); note as a future spatial-index optimization.
- [Editor overlay currently uses legacy DebugRenderer] -> Route overlay through the aurora render helper; if unavailable, keep overlay logic render-free and let the aurora editor layer draw it.
- [Height precision across tile borders] -> Adopt the 1-texel tile overlap convention so bilinear sampling and finite differences do not seam.
- [Bullet heightfield local-space conventions] -> `btHeightfieldTerrainShape` is centered on origin with unit grid spacing and no transform scale through the current `CollisionObject`; either apply shape local scaling to `resolution` or bake world-space samples plus a centered offset, and handle the up-axis and quad-edge flip.
- [Recast only reads `CollisionComponent` triangle meshes] -> A heightfield collider is invisible to the navigation generator; expose terrain height samples (or emitted triangles) from the core so navigation can rasterize terrain without depending on render, and resolve the exact path in the open questions.
- [No runtime `PhysicsWorld` today] -> Physics currently attaches only in the editor `Document`; add a runtime attach path and enable the physics backend module in the game module config so terrain collision is live outside the editor.
- [Heightfield sample conversion] -> R16_UNORM tile samples (or R32_SFLOAT) must be converted to the Bullet heightfield scalar type; pick one conversion and test it against `QueryHeight`.

## Migration Plan

1. Land `engine/terrain` core + data model + tests without touching render (additive).
2. Rewrite `plugins/terrain` render layer to aurora on top of the core; remove the legacy `RenderAdaptor`/`rhi` links from terrain logic.
3. Rebuild `TerrainComponent`/`TerrainData` and update serialization; no production terrain data exists to migrate.
4. Consume the heightfield shape from `physics-backend-abstraction`, then add the per-tile terrain collision layer and the runtime physics attach.
5. Re-bind editor tooling and generator; delete `tools/` and stale test.
6. Rollback strategy: phases are independent; reverting the render or collision phase leaves the core and tests intact.

## Open Questions

- Final naming for the core runtime field/asset types (`TerrainField`, `TerrainData`, terrain asset type string).
- Whether the offline builder ships in this change or lands immediately after the core (needed for headless tests, prefer in-change).
- Whether splatmap tile grouping is fixed at 4 layers per RGBA tile or configurable, and whether splatmaps are stored on all LODs or only the near/high-detail LODs (prefer near-only).
- Default `lodCount` and the mapping from clipmap level to tile LOD (e.g. `lod = min(level, lodCount - 1)`), plus how per-LOD annulus radii relate to the clipmap ring extents.
- Whether LOD transitions use skirts, geomorphing, or transition blocks.
- Navigation geometry: resolved to sampling the terrain core directly (LOD0) through a geometry-provider seam, not the physics heightfield; tracked by the `terrain-navigation-integration` change.
