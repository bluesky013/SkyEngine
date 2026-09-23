## 1. Aurora adaptor

- [ ] 1.1 Create the `plugins/vegetation` aurora adaptor and module registration, registering it with the core factory seam
- [ ] 1.2 Implement GPU population of instances from the density field with core-provided plain data
- [ ] 1.3 Implement instanced (near) and merged/billboard (far) tiers with density LOD fade
- [ ] 1.4 Implement the wind field deformation
- [ ] 1.5 Implement character interaction (bounded actor buffer/texture) pushing vegetation
- [ ] 1.6 Implement the dedicated foliage-lit pass with shadow participation
- [ ] 1.7 Add vegetation shaders/techniques/materials under `assets/shaders/vegetation/`, `assets/techniques/`, `assets/materials/`

## 2. Culling

- [ ] 2.1 Implement cell-level frustum + distance culling (conservative bounds inflated by max vegetation height)
- [ ] 2.2 Implement GPU per-instance frustum cull in the population pass with per-cell indirect draw args
- [ ] 2.3 Skip submitting/indirect-drawing empty (fully culled) cells
- [ ] 2.4 Validate culling: cells outside the frustum/distance are not drawn, empty cells emit no draw

## 3. Validation

- [ ] 3.1 Build the engine and confirm no vegetation/render regressions
