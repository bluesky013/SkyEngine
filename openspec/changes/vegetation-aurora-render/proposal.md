> **Current status — 2026-11-01: planned.** 运行时植被渲染、LOD、风场与 culling 尚待实施；基础 Renderer 已交付。
> Owner: `vegetation-aurora-render`.

## Why

`vegetation-large-world` delivered the render-agnostic core (surface seam, placement, streaming, asset, component) and the render adaptor seam (`VegetationRenderFactory` / `IVegetationRenderAdaptor`). This change implements the aurora render adaptor plus the culling scheme.

## What Changes

- Register an aurora vegetation render adaptor with the core factory seam.
- GPU population of instances from the density/instance data with core-provided plain data.
- Instanced (near) and merged/billboard (far) tiers with density LOD fade.
- Wind field deformation and character interaction (bounded actor buffer/texture).
- Dedicated foliage-lit pass with shadow participation; vegetation shaders/techniques/materials.
- Cell-level frustum + distance culling, GPU per-instance frustum cull with per-cell indirect draw args, skip empty cells.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `vegetation`: aurora vegetation render adaptor (GPU population, tiers, wind, interaction, foliage pass, culling).

## Impact

- `plugins/vegetation` aurora adaptor + module; vegetation shader/technique/material assets; depends on `engine/vegetation` seam + aurora.
- No new third-party dependencies.
