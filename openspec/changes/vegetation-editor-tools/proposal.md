> **Current status — 2026-11-01: blocked-on-prereq.** 等待 vegetation render 的可用预览/绘制闭环。
> Owner: `vegetation-editor-tools`. Prerequisites: `vegetation-aurora-render`.

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

None.

### Modified Capabilities

- `vegetation`: vegetation authoring extension on the aurora sandbox framework.

## Impact

- `engine/sandbox` extension for vegetation (non-Qt); depends on `engine/vegetation` + `plugins/vegetation` and the aurora sandbox render.
- No new third-party dependencies.
