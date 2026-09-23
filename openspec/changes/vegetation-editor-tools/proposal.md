## Why

Vegetation editor tooling is implemented on the aurora sandbox framework (`engine/sandbox` `EditorExtension`, non-Qt), not the legacy Qt editor. It was split out of `vegetation-large-world` so the core can land independently.

## What Changes

- Implement a vegetation editor extension on the aurora sandbox framework.
- Biome authoring (rules, palette, density).
- Density painting/erasing into world-space density maps.
- Editor preview using the same deterministic placement as runtime.
- Bake the configuration into the vegetation asset.

## Capabilities

### New Capabilities

- `vegetation-editor`: vegetation authoring extension on the aurora sandbox framework.

### Modified Capabilities

<!-- none -->

## Impact

- `engine/sandbox` extension for vegetation (non-Qt); depends on `engine/vegetation` + `plugins/vegetation` and the aurora sandbox render.
- No new third-party dependencies.
