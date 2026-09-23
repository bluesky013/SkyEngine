## Context

The render layer was designed in `terrain-large-world-core` (now archived): D7 (aurora render layer: feature processor, atlas, clipmap instancing, material), D13 (LOD seam stitching: skirt default, geomorph optional, same for tile-LOD adjacency), D14 (GPU-driven terrain node: residency=CPU, selection/visibility/draw=GPU, with a CPU fallback). This change implements those decisions on aurora.

## Goals / Non-Goals

**Goals:**

- Implement the aurora terrain render layer (feature processor, per-LOD atlases, clipmap instancing, stitching, material).
- Implement the optional GPU-driven path (resident tile buffer, culling, indirect) with a CPU fallback.

**Non-Goals:**

- Changing the core/data/streaming (owned by `terrain-large-world-core`).
- Editor tooling (owned by `terrain-editor-tools`).

## Decisions

- Follow the archived decisions D7/D13/D14 from `terrain-large-world-core` (referenced, not repeated here).
- Renderer target: aurora scene/RDG; the render layer consumes plain data from `ITerrainSystem` (no core render types).

## Risks / Trade-offs

- [Aurora API maturity] -> keep the core/render seam plain-data; fall back to a simpler tier when a facility (e.g. indirect) is missing.
- [GPU-driven buffer/atlas lifecycle] -> manage slots and defragmentation in the render layer.

## Migration Plan

1. aurora adaptor + feature processor + clipmap + atlas + material.
2. Stitching (skirt), then optional geomorphing.
3. GPU-driven path; keep the CPU fallback.
