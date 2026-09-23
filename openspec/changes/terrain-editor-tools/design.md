## Context

Editor tooling for terrain targets the aurora sandbox framework (`engine/sandbox`), which provides a non-Qt editor core (`EditorExtension`, `PanelRegistry`, `PropertyModel`, `DocumentService`, `Viewport`) and an aurora-based render layer. The legacy Qt editor is not used.

## Goals / Non-Goals

**Goals:**

- Author terrain (create, grid add/remove, generate/bake) through sandbox editor extensions bound to the core component/data.
- Preview terrain using core data.

**Non-Goals:**

- Changing the core/data/streaming (owned by `terrain-large-world-core`).
- The render layer (owned by `terrain-aurora-render`).

## Decisions

- Editor tooling is an `EditorExtension` on `engine/sandbox` (non-Qt), not the legacy Qt editor.
- Authoring operates on the refactored `TerrainComponent` and `engine/terrain` data; generation calls the core generator/builder.

## Risks / Trade-offs

- [Sandbox API in flux] -> keep the extension thin over core data; isolate sandbox-specific UI.
- [Preview parity] -> use the same core generation/placement as runtime.

## Migration Plan

1. Terrain sandbox editor extension scaffold bound to the component.
2. Create Terrain + grid add/remove.
3. Generator config/preview/bake UI; sculpt/paint seam.
