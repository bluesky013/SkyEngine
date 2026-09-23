## Why

The terrain editor tooling was re-scoped away from the legacy Qt editor to the aurora sandbox framework (`engine/sandbox` `EditorExtension`, non-Qt). It was split out of `terrain-large-world-core` so the core can land independently. This change implements terrain authoring as a sandbox editor extension.

## What Changes

- Implement a terrain editor extension on the aurora sandbox framework bound to the refactored `TerrainComponent` and core data model.
- Restore the "Create Terrain" path (actor + component with selected metadata).
- Implement grid add/remove that mutates the terrain tile set and persists it.
- Make the generator a thin config/preview/bake UI over the core `TerrainGenerateConfig`/generator, producing splatmaps and the per-tile LOD chain.
- Route editor overlay drawing through the aurora render helper consuming core data.
- Add the documented sculpt/paint reserved seam.

## Capabilities

### New Capabilities

- `terrain-editor`: terrain authoring extension on the aurora sandbox framework.

### Modified Capabilities

<!-- none -->

## Impact

- `engine/sandbox` extension for terrain (non-Qt); depends on `engine/terrain` + `plugins/terrain` and the aurora sandbox render.
- No new third-party dependencies.
