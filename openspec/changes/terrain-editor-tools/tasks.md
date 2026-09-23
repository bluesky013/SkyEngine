## 1. Editor extension

- [ ] 1.1 Implement a terrain editor extension on the aurora sandbox framework (`engine/sandbox` `EditorExtension`, non-Qt) bound to the refactored component and core data model
- [ ] 1.2 Restore the "Create Terrain" path (actor + component with selected metadata)
- [ ] 1.3 Implement grid add/remove that mutates the terrain tile set and persists it
- [ ] 1.4 Make the generator a thin config/preview/bake UI over the core `TerrainGenerateConfig` and core generator, producing splatmaps and the per-tile LOD chain
- [ ] 1.5 Route editor overlay drawing through the aurora render helper consuming core data
- [ ] 1.6 Add the documented sculpt/paint reserved seam (interface + inert default)

## 2. Validation

- [ ] 2.1 Build the sandbox editor and confirm terrain authoring works end to end
