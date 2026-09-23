## Why

The engine has no vegetation system. A large open world needs grass, foliage, and ground cover that (a) covers tens of km² without an artist placing every instance, (b) streams with the camera and stays performant, and (c) reacts to wind and characters. The proven approach at this scale is the one used by open-world titles like Ghost of Tsushima: procedural, GPU-populated instanced vegetation driven by a density/biome field, streamed in cells around the camera, with distance-based density LOD, wind, and character interaction, rendered in a dedicated foliage-lit pass.

Two decoupling requirements drive this change, mirroring what the engine already does for navigation and terrain:

1. Vegetation logic must not be tied to a specific world surface. Depending directly on `engine/terrain` would prevent reuse (custom surfaces, tests, other heightfield sources). Vegetation should consume an abstract **surface provider**; terrain is one implementation.
2. Vegetation logic must not be tied to the renderer. The current render target is aurora, but the core must stay render-free and the renderer must plug in behind an **adaptor seam**, so a different renderer (or a headless/test renderer) can be supplied.

Both seams mirror the established patterns: `engine/navigation` defines a geometry-provider seam consumed by backends, and `engine/navigation`/`engine/terrain` keep their cores free of render/RHI types while renderers live in separate layers.

## What Changes

- Add a render-agnostic vegetation core module (`engine/vegetation`, target links `Framework` only — no `Terrain`, no `render/`, no `rhi/`) holding the biome/palette model, deterministic placement rules, density/LOD rules, per-world subsystem, and streaming state.
- Add a **surface provider seam**: the core defines an abstract `IVegetationSurfaceProvider` (world-space height, slope, layer/splat sampling, cell bounds, and change notification); a terrain bridge implements it (linking `Terrain` + `Vegetation`) so terrain is one pluggable surface.
- Add a **render adaptor seam**: the core defines a render adaptor/factory seam and hands over plain data; the aurora vegetation plugin implements the adaptor, so the core never includes or links aurora/render types.
- Add a vegetation asset format: biome sets, species/palette definitions, density (distribution) maps, and placement rules, with binary serialization and a source.
- Place vegetation deterministically from the surface provider (height, slope, layer weights) plus density maps, so the same inputs always produce the same distribution.
- Add focus-driven vegetation streaming in cells (bounds supplied by the provider, aligned to terrain tiles when the provider is terrain), with per-cell density LOD bands, off-tick async prefetch, and per-world isolation.
- Add the aurora render adaptor: GPU-driven population, instanced/billboard tiers with density LOD fade, wind, character interaction, a dedicated foliage-lit pass, and material/technique binding.
- Add editor tooling for biome/density authoring, painting, preview, and bake.
- Keep the dependency direction one-way: vegetation core -> abstract surface; terrain/aurora are implementations plugged in from outside.

## Capabilities

### New Capabilities

- `vegetation-core`: render-agnostic `engine/vegetation` module (links `Framework` only), biome/palette model, deterministic placement rules, density/LOD rules, per-world vegetation subsystem lifecycle, and the render-agnostic handoff.
- `vegetation-surface`: the abstract surface-provider seam (height/slope/layer sampling, cell bounds, change notification) and the terrain-backed bridge implementation.
- `vegetation-data`: vegetation asset format (biome sets, species/palette, density maps, placement rules), binary serialization round-trip, source, and the logic-only vegetation component.
- `vegetation-streaming`: focus-driven cell paging, per-cell distance density LOD, per-tick budget, off-tick async prefetch, per-world isolation, and invalidation on surface change.
- `vegetation-render`: the render adaptor seam (factory/handoff) plus the aurora implementation — GPU population, instanced/billboard tiers, wind, character interaction, and a dedicated foliage-lit pass.
- `vegetation-editor`: biome/density authoring and painting, preview, and bake to the vegetation asset.

### Modified Capabilities

<!-- No vegetation capabilities exist under openspec/specs/. -->

## Impact

- **New**: `engine/vegetation` (core module, links `Framework` only), a terrain surface bridge (links `Terrain` + `Vegetation`), and an aurora vegetation render adaptor under `plugins/vegetation`.
- **Depends on** `terrain-large-world-core` only through the terrain surface bridge (world-space addressing, LOD0 samples, splat weights, terrain-tile streaming/change notifications); the core itself has no terrain dependency. A terrain read API (LOD0 over a bounds) and change notification are shared with `terrain-navigation-integration`.
- **`plugins/vegetation`**: new plugin (aurora render adaptor + editor tools) registered in `plugins/plugins.json` / `plugin.json` and the runtime module configs.
- **Assets**: vegetation biome/palette assets and shaders/techniques under `assets/vegetation/`, `assets/shaders/vegetation/`, `assets/techniques/`.
- **Downstream**: gameplay/AI can query vegetation density/presence through the core; rendering goes through the adaptor seam; the surface comes from the provider seam. Physics collision for vegetation is out of scope.
- **Module boundary**: per `terrain-vegetation-plugin-restructure`, `engine/vegetation` holds only the surface seam + data/interface; the vegetation implementation lives in `plugins/vegetation`.
- No new third-party dependencies.
