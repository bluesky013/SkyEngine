## Context

The vegetation core is done: surface provider seam, deterministic placement, cell streaming, asset/component, and the render adaptor seam (`engine/vegetation/VegetationRenderAdaptor.h`: `IVegetationRenderAdaptor`, `VegetationRenderFactory`, plain-data `VegetationRenderCell`). The render layer consumes plain data via the seam.

## Goals / Non-Goals

**Goals:**

- Implement the aurora adaptor (GPU population, tiers, wind, interaction, foliage-lit pass, culling).
- Keep the core free of render types.

**Non-Goals:**

- Changing core placement/streaming/asset/component.
- Editor tooling (owned by `vegetation-editor-tools`).

## Decisions

- The adaptor is registered with `VegetationRenderFactory`; the system pushes `VegetationRenderCell` on cell load/unload (already implemented in the core).
- Culling: cell-level frustum+distance on CPU, GPU per-instance frustum cull with per-cell indirect draw args; empty cells skip.

## Risks / Trade-offs

- [Aurora facility availability] -> fall back to a simpler tier if indirect/compute is missing.
- [Overdraw] -> density LOD thinning + tiers with fades + dedicated pass.

## Migration Plan

1. Aurora adaptor + module registration.
2. GPU population + tiers + material.
3. Wind + interaction + foliage-lit pass.
4. Culling (cell + GPU instance) + validation.
