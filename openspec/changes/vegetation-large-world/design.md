## Context

The engine has no vegetation system. The world surface is provided by the terrain large-world change: a render-agnostic core (`engine/terrain`) with world-space tiled addressing, per-tile LOD chains, LOD0 height samples, splat/layer weights, focus-driven streaming, and per-world state. Navigation and physics consume terrain through one-way dependencies, and renderers live in separate layers behind adaptors (aurora). `engine/navigation` additionally demonstrates a **provider seam** (`INaviGeometryProvider`) so a consumer/backend consumes geometry sources it does not link against.

Large-world vegetation (grass, ground cover, small foliage) has well-established requirements, learned from open-world titles such as Ghost of Tsushima:

- **Procedural population**: instances generated from a density/biome field rather than placed per instance.
- **GPU-driven**: population and drawing on the GPU to keep CPU cost flat.
- **Streaming and density LOD**: cells around the camera with density decreasing by distance.
- **Wind and interaction**: global wind plus per-instance response, and characters pushing vegetation.
- **Dedicated folliage-lit pass**: grass lit distinctly (blade attenuation/translucency), not as opaque geometry.

Constraints: the engine core modules avoid render/RHI includes and keep renderers behind adaptors; per-world isolation is required (like terrain/navigation); the active renderer is aurora; terrain LOD0 is the authoritative surface.

## Goals / Non-Goals

**Goals:**

- Provide a render-agnostic vegetation core (`engine/vegetation`) that links only `Framework` — no terrain, render, or RHI dependency.
- Decouple the core from the world surface via an abstract **surface provider** seam; terrain is one implementation supplied by a bridge.
- Decouple the core from the renderer via a **render adaptor seam**; aurora is one implementation supplied by a plugin.
- Produce a deterministic, streamable vegetation distribution with density LOD, wind, and interaction.
- Keep dependency directions one-way: core -> abstract surface; renderer -> core.

**Non-Goals:**

- Physics collision for vegetation.
- A large-tree/forest system with full mesh assets, destruction, or authoring (follow-up).
- GPU indirect culling/cluster rendering beyond vegetation needs.
- Authoring every palette/species model (a minimal set plus the pipeline).
- Networking/replication of vegetation state.

## Decisions

### D1: Layering -- `engine/vegetation` core + `plugins/vegetation` aurora adaptor

**Decision**: Create `engine/vegetation` (STATIC, `LINK_LIBS Framework` only) holding the render-agnostic core, and a `plugins/vegetation` plugin containing the aurora render adaptor plus editor tools. The core never includes `render/`, `rhi/`, or `terrain/` headers, and the plugin owns all GPU state.

**Rationale**: Same split as `engine/navigation` + `plugins/recast` and `engine/terrain` + `plugins/terrain`; keeps the core reusable and the renderer swappable.

**Alternatives considered**: Single render-only plugin (coupled to aurora). Core linking `Terrain` directly (coupled to one surface; blocks tests and other surfaces).

### D2: Surface provider seam (decouple core from terrain)

**Decision**: The core defines an abstract `IVegetationSurfaceProvider` and consumes it; the core does not link terrain. The provider supplies, for a world-space position or region: height, slope (or enough to derive it), terrain layer/splat weights, cell bounds for streaming, and a change notification when the surface changes. A terrain bridge (linking `Terrain` + `Vegetation`) implements the provider using terrain LOD0 data; tests can supply a synthetic provider.

```
IVegetationSurfaceProvider {
    float  SampleHeight(const Vector3 &worldPos, bool &outValid) const;
    Vector3 SampleNormal(const Vector3 &worldPos, bool &outValid) const;
    void   SampleLayers(const Vector3 &worldPos, /*out*/ LayerWeights &out) const;
    void   GetCellBounds(TerrainStyleCellCoord cell, /*out*/ AABB &bounds) const;   // naming TBD
    void   GetCellRange(const AABB &bounds, /*out*/ CellRange &out) const;
    void   AddSurfaceChangedListener(IVegetationSurfaceListener *listener);          // invalidation
}
```

**Rationale**: Mirrors `INaviGeometryProvider`: vegetation placement is defined against an abstract surface, so terrain is a plug-in and the core stays engine-core-clean. This is the exact decoupling the engine uses for navigation geometry.

**Alternatives considered**: Link `engine/terrain` in the core (simple but coupled to one surface and to the terrain module's evolution). Read the surface through physics (wrong direction, backend-specific).

### D3: Render adaptor seam (decouple core from aurora)

**Decision**: The core defines a render adaptor seam: a `VegetationRenderFactory`/`IVegetationRenderAdaptor` contract plus the plain-data handoff (cell bounds, biome/palette parameters, density data reference, surface sampling handle). The aurora plugin registers an implementation. The core never includes or links aurora/render/RHI types.

**Rationale**: Mirrors the backend factory pattern (`NaviMeshFactory::Impl`, terrain's render adaptor). It lets aurora be replaced (or a headless/test adaptor be used) without touching the core, and keeps the core render-free per `vegetation-core`.

**Alternatives considered**: Aurora-specific code in the core (violates render decoupling). Plain-data handoff with no factory seam (renderer not pluggable; acceptable for terrain but the requirement here is an explicit adaptor seam).

### D4: Terrain-driven deterministic placement

**Decision**: Placement is a pure function of provider-sampled height, slope, and layer weights, plus density maps and a seed, evaluated in world space per cell. Same inputs -> identical distribution, independent of order.

**Rationale**: The surface provider is the surface of record; determinism makes results cacheable, lets the editor preview match runtime, and keeps GPU population a deterministic function of the same inputs.

**Alternatives considered**: Author every instance (infeasible). Sample a specific terrain LOD directly (couples to terrain and is non-deterministic across LOD changes).

### D5: Vegetation asset and component

**Decision**: Define a vegetation asset (biome set, species/palette definitions, world-space density maps, rules) with a source and a logic-only `VegetationComponent` (asset/source id, biome set, seed, parameters) resolved through `VegetationSystem`, never a render or terrain object.

**Rationale**: Matches the terrain/navigation asset + component pattern and keeps the component plain-data.

**Alternatives considered**: Hardcode one grass type. Store placement in scene data.

### D6: Cell streaming via provider-supplied bounds

**Decision**: Vegetation streams in cells whose bounds come from the provider (`GetCellBounds`/`GetCellRange`); for the terrain bridge these align to terrain tiles. `UpdateStreaming` loads/unloads cells around a focus with hysteresis and a per-tick budget, prefetches inputs off-tick, keeps state per world, and invalidates overlapping cells on surface change.

**Rationale**: Reuses the provider's addressing (terrain tiles, or any surface's cells), keeps vegetation and surface residency consistent, and gives per-world isolation.

**Alternatives considered**: A vegetation-private global grid (extra mapping, drift from the surface).

### D7: GPU-driven population

**Decision**: The render adaptor generates instances on the GPU from the density field, consuming the plain-data handoff; the core never allocates GPU resources.

**Rationale**: CPU population of millions of blades is infeasible; the core/render seam stays plain-data.

**Alternatives considered**: CPU population per frame. Fully baked instances.

### D8: Density LOD and render tiers

**Decision**: Density decreases with distance (per-cell density LOD bands); rendering tiers switch near instanced meshes -> mid reduced instances -> far merged/billboard patches with fades. The core owns the LOD/density rules as data; the adaptor implements the tiers.

**Rationale**: Overdraw dominates grass cost; thinning and merging by distance makes large fields affordable.

**Alternatives considered**: Uniform density. Hard cutoffs (popping).

### D9: Wind and character interaction

**Decision**: A global wind field drives per-instance deformation; character interaction writes an interaction buffer/texture (position/radius/strength) that bends nearby vegetation, with a bounded actor count. Parameters live in the core; deformation and the buffer are adaptor-side.

**Rationale**: Wind and interaction are the defining features of the reference system and are cheap in the vertex/population stage.

**Alternatives considered**: Static vegetation. Per-instance CPU simulation.

### D10: Foliage-lit dedicated pass

**Decision**: Vegetation renders in a dedicated pass with foliage lighting (blade attenuation/translucency) and shadow participation, using vegetation techniques/materials, implemented by the adaptor.

**Rationale**: Foliage lighting makes grass read correctly; a dedicated pass contains the model and overdraw handling.

**Alternatives considered**: Opaque material. Full reference-grade grass lighting from the start (keep a simpler attenuation model first with a seam).

### D11: Editor authoring

**Decision**: A vegetation tool authors biomes, paints density into world-space maps, previews placement/streaming using the same deterministic core placement, and bakes the vegetation asset/source.

**Rationale**: Enables authoring while keeping preview and runtime consistent.

**Alternatives considered**: File-only authoring. Editor-only placement.

### D12: Registration owns the coupling

**Decision**: The terrain surface provider and the aurora render adaptor are registered from outside the core: the terrain bridge registers/creates the surface provider; the aurora plugin registers the render adaptor with the factory seam. The core holds opaque lists/pointers to these abstractions.

**Rationale**: Preserves one-way layering; only the bridge knows terrain, and only the plugin knows aurora.

**Alternatives considered**: Core creates the terrain provider (core would link terrain). Core creates the aurora adaptor (core would link render).

### D13: Vegetation culling

**Decision**: Cull vegetation at two granularities, both render-side:

- **Cell-level (CPU)**: each loaded cell has an AABB (from the provider) inflated by the maximum vegetation height; cells outside the camera frustum are not submitted. Distance culling uses the streaming bands, so cells beyond the unload radius are already gone. Frustum culling does not unload cells; it only skips draw submission.
- **Instance-level (GPU)**: the population pass frustum-culls each candidate instance during population and atomically appends survivors to the cell's instance buffer, so the CPU never iterates instances. Per-cell indirect draw arguments carry the resulting instance count, so a fully culled cell issues a zero-instance (effectively skipped) draw.
- **Bounds are conservative**: cell AABB + max species height, so near-plane and side clipping do not pop instances.
- **Occlusion** (GPU HiZ) is deferred; the seam (per-cell indirect args) leaves room for it.

**Rationale**: CPU instance culling is infeasible at vegetation counts; cell culling bounds the number of population dispatches, and GPU instance culling removes the remaining off-screen instances cheaply. Indirect draws let empty cells cost nothing. Keeping culling render-side preserves that core residency/queries are independent of the camera (matching `vegetation-core` / `vegetation-streaming`).

**Alternatives considered**: CPU per-instance culling (too slow). No culling (overdraw dominates; contradicts the density-LOD goal). Occlusion culling now (needs HiZ infrastructure; deferred behind the same seam).

## Risks / Trade-offs

- [Indirection cost of the surface provider] -> Placement samples the provider per point; keep the provider interface cheap (batched region sampling where possible) and cache per-cell samples.
- [Provider/adaptor lifetime and threading] -> Providers and adaptors must be safe to call from build/populate workers; document read-only access expectations.
- [GPU population cost and overdraw] -> Density LOD thinning, tiers with fades, dedicated pass, bounded per-cell dispatch.
- [Determinism across CPU preview and GPU population] -> Fix the placement/RNG formula in the core and share it; the GPU population is a deterministic function of the same inputs.
- [Interaction buffer cost] -> Cap interacting actors and texture resolution; only near cells sample it.
- [Aurora API maturity] -> Keep the adaptor seam plain-data; fall back to a simpler tier if a needed facility is missing.
- [Scope creep into trees/forests] -> Keep grass/ground cover + instancing; trees are a follow-up.
- [Surface change storm] -> Coalesce surface-change notifications per tick before invalidating cells.

## Migration Plan

1. Land `engine/vegetation` core (surface provider abstraction, biome model, placement rules, LOD rules, subsystem, streaming) with tests using a synthetic surface provider.
2. Add the vegetation asset format + source + logic-only component.
3. Add the terrain surface bridge (links `Terrain` + `Vegetation`) implementing the provider.
4. Add the `plugins/vegetation` aurora render adaptor (factory registration, GPU population, tiers, wind, interaction, dedicated pass).
5. Add editor tooling.
6. Rollback: the core + synthetic provider tests are independent of terrain and aurora; reverting the bridge or adaptor leaves the core intact.

## Open Questions

- Exact `IVegetationSurfaceProvider` shape (region/batched sampling vs per-point; how slope/layers are exposed; cell coordinate type shared with terrain or vegetation-local).
- Whether the render adaptor seam is a factory (`Impl` like navigation) or a registered interface instance, and what the handoff struct contains.
- Cell size: provider cell 1:1 with terrain tiles or subdivided; who decides.
- Density LOD math/thresholds and whether it is per-biome or global.
- Interaction model specifics (buffer vs texture, actor budget, who writes it).
- Whether far-field merged patches are baked or generated per cell.
- Whether large trees reuse this system (instanced static meshes) or a separate system.
