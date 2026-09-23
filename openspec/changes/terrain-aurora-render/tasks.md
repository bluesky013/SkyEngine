## 1. Aurora render adaptor

- [ ] 1.1 Create the aurora terrain render adaptor target and module registration
- [ ] 1.2 Implement the aurora terrain feature processor that pulls core render data (metadata, visible (tile, LOD) set, payloads) each frame
- [ ] 1.3 Move clipmap block mesh, per-level layout, and snap-to-grid camera tracking into the render layer, mapping each ring to a tile LOD
- [ ] 1.4 Implement the per-LOD GPU heightmap/splatmap atlases with upload on tile-LOD load and release/reuse on unload
- [ ] 1.5 Implement instanced clipmap block rendering (sampling the ring's LOD atlas) through the aurora scene/RDG

## 2. Stitching and material

- [ ] 2.1 Implement skirt-based LOD seam stitching for clipmap block edges
- [ ] 2.2 Implement optional vertex geomorphing between adjacent LOD levels
- [ ] 2.3 Apply the same stitching (skirt/geomorph) to adjacent tiles rendered at different LODs
- [ ] 2.4 Verify stitching does not affect core data, CPU queries, or collision
- [ ] 2.5 Bind the terrain material and color/depth/shadow techniques with height scale/offset and layer parameters
- [ ] 2.6 Update `assets/shaders/terrain/`, `assets/techniques/`, and `assets/materials/` for the LOD atlas-based tile model

## 3. GPU-driven path

- [ ] 3.1 Implement the resident tile buffer and per-LOD atlas slot table with incremental updates from the core residency delta
- [ ] 3.2 Implement GPU compute block/tile frustum culling and LOD/ring selection
- [ ] 3.3 Implement instance compaction and indirect draw argument generation
- [ ] 3.4 Implement the GPU-driven instanced draw sampling the selected LOD atlas
- [ ] 3.5 Retain and validate the CPU-driven fallback path

## 4. Validation

- [ ] 4.1 Add a module load/factory test for the render module
- [ ] 4.2 Build the engine and confirm no terrain/render compile or link regressions
