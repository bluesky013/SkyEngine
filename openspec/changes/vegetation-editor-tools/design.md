## Context

Editor tooling for vegetation targets the aurora sandbox framework (`engine/sandbox`), a non-Qt editor core (extensions, panels, property model, viewport) with an aurora render layer. The legacy Qt editor is not used.

## Goals / Non-Goals

**Goals:**

- Author biomes, paint density, preview placement, and bake the vegetation asset as sandbox editor extensions.

**Non-Goals:**

- Changing core placement/streaming/asset.
- The render layer (owned by `vegetation-aurora-render`).

## Decisions

- Editor tooling is an `EditorExtension` on `engine/sandbox` (non-Qt).
- Preview uses the same deterministic core placement as runtime; bake calls the core/vegetation baker.

## Risks / Trade-offs

- [Sandbox API in flux] -> keep the extension thin over core data; isolate sandbox-specific UI.
- [Preview parity] -> single placement implementation in the core.

## Migration Plan

1. Vegetation sandbox editor extension scaffold.
2. Biome authoring + density painting.
3. Preview + bake.
