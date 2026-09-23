## 1. Core module scaffold (`engine/vegetation`)

- [x] 1.1 Create `engine/vegetation/CMakeLists.txt` with target `Vegetation` as STATIC linking `Framework` only, plus the `include`/`src` layout
- [x] 1.2 Add the `Vegetation` target to the engine build graph and confirm it has no `Terrain`, `render/`, `rhi/`, or `RenderAdaptor` dependency
- [x] 1.3 Add a `VegetationTest` test target alongside `TerrainTest`
- [x] 1.4 Verify the core compiles and links standalone without terrain and without the aurora/render layers

## 2. Surface provider seam

- [x] 2.1 Define `IVegetationSurfaceProvider` (height/normal/layer sampling, cell bounds/range, change listeners) using only engine-core types
- [x] 2.2 Define the surface-change listener/notification contract
- [x] 2.3 Add a synthetic surface provider for tests
- [x] 2.4 Add tests that placement/streaming run against a synthetic provider and handle an absent provider
- [x] 2.5 Create the terrain surface bridge target (links `Terrain` + `Vegetation`) implementing the provider from terrain LOD0
- [x] 2.6 Implement cell bounds aligned to terrain tiles and surface-change notification from terrain tile changes in the bridge

## 3. Biome, palette, and placement rules

- [x] 3.1 Define biome, species/palette, scale/rotation jitter, wind response, and density types in `engine/vegetation`
- [x] 3.2 Implement deterministic world-space placement from provider-sampled height, slope, and layer weights plus density maps and a seed
- [x] 3.3 Implement placement rules (layer/splat gating, max slope, density modulation) as data
- [x] 3.4 Expose vegetation presence/density queries over world positions
- [x] 3.5 Add tests for determinism, rule gating, and distance density
- [x] 3.6 Add an instance path: the asset carries an instance list and cells containing instances use them; cells without instances use procedural placement

## 4. Vegetation asset and component

- [x] 4.1 Define the vegetation asset payload: biome set, palette, world-space density map payloads, and rules
- [x] 4.2 Implement binary save/load and register the asset type/handler
- [x] 4.3 Add a serialization round-trip test
- [x] 4.4 Define the vegetation source type with reflection
- [x] 4.5 Define and register the logic-only `VegetationComponent` (asset/source id, biome set, seed, density/LOD/streaming params)

## 5. Vegetation sub-system and streaming

- [x] 5.1 Implement `VegetationSystem` as `IWorldSubSystem` owning the biome config, provider handle, and streaming state per world
- [x] 5.2 Implement focus-driven cell paging using provider cell bounds with load/unload hysteresis and a per-tick budget
- [x] 5.3 Implement per-cell distance density LOD bands with fades
- [x] 5.4 Implement off-tick async cell preparation and pending cancellation
- [x] 5.5 Require provider surface data for a cell and defer/retry when unavailable
- [x] 5.6 Invalidate overlapping cells on provider surface-change notification
- [x] 5.7 Add the render-adaptor plain-data handoff (cell bounds, biome/palette params, density reference, surface handle)
- [x] 5.8 Add tests for paging, density LOD, budget, isolation, and invalidation

## 6. Aurora render adaptor (`plugins/vegetation`)

- [x] 6.1 Define the render adaptor/factory seam in the core (no render types) and register/consume an implementation
- [ ] 6.2 Create the `plugins/vegetation` plugin with the aurora adaptor and module registration, registering it with the seam
- [ ] 6.3 Implement GPU population of instances from the density field with core-provided plain data
- [ ] 6.4 Implement instanced (near) and merged/billboard (far) tiers with density LOD fade
- [ ] 6.5 Implement the wind field deformation
- [ ] 6.6 Implement character interaction (bounded actor buffer/texture) pushing vegetation
- [ ] 6.7 Implement the dedicated foliage-lit pass with shadow participation
- [ ] 6.8 Add vegetation shaders/techniques/materials under `assets/shaders/vegetation/`, `assets/techniques/`, `assets/materials/`
- [x] 6.9 Register the plugin in `plugins/plugins.json`, `plugin.json`, and the runtime module configs
- [x] 6.10 Confirm the core `Vegetation` target links with neither `Terrain` nor the aurora render layer
- [ ] 6.11 Implement cell-level frustum + distance culling (conservative bounds inflated by max vegetation height)
- [ ] 6.12 Implement GPU per-instance frustum cull in the population pass with per-cell indirect draw args
- [ ] 6.13 Skip submitting/indirect-drawing empty (fully culled) cells
- [ ] 6.14 Validate culling: cells outside the frustum/distance are not drawn, empty cells emit no draw

## 7. Editor and validation

- [ ] 7.1 Implement biome authoring (rules, palette, density) in a vegetation editor extension on the aurora sandbox framework (`engine/sandbox` `EditorExtension`, non-Qt)
- [ ] 7.2 Implement density painting/erasing into world-space density maps
- [ ] 7.3 Implement editor preview using the same deterministic placement as runtime
- [ ] 7.4 Implement bake of the configuration into the vegetation asset
- [x] 7.5 Run the vegetation tests (placement, streaming, asset round-trip, synthetic provider) and confirm they pass
- [x] 7.6 Build the engine (desktop) and confirm no compile/link regressions
