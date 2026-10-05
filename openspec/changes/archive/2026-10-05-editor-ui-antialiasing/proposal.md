## Why

The editor UI looks coarse: rounded panels/fields/buttons/checkboxes/knobs are drawn as per-scanline
axis-aligned rectangles, so their corners are hard stair-steps with no anti-aliasing. MSAA is out of
scope for now, and the UI shader only does `color * texture.Sample`. Also, the theme/skin layer
(`UiTheme`/`UiSkin`/`UiDraw`) has no spec coverage. This change adds non-MSAA anti-aliased shape drawing
to the style layer.

## What Changes

- Add a **procedural, anti-aliased rounded-rectangle** to the UI: bake a small 9-slice corner texture
  (with AA edges/borders) and draw rounded fills/borders via `AddTexturedQuad` instead of scanline
  rectangles.
- Route `UiDraw::RoundedRect` / `RoundedField` / `RoundedGradient` / `UiSkin` components through the
  AA path (keep a `AddRect` fallback when no texture registry is available).
- Cover the theme/skin layer with a capability (`editor-ui-styling`) so styling and its AA rendering
  rules are specified.

## Capabilities

### New Capabilities

- `editor-ui-styling`: the theme/skin layer (`UiTheme`, `UiSkin`, `UiDraw`) and its anti-aliased shape
  rendering (rounded rectangles/fields without MSAA).

### Modified Capabilities
<!-- none -->

## Impact

- `engine/sandbox/shell`: `UiDraw` gains an AA rounded-rect path backed by a generated 9-slice texture;
  `UiSkin` continues to use it. No engine/RHI changes; no MSAA.
- `engine/ui`: may reuse `IUITextureRegistry` (already exposed) to register the corner texture.
- No new third-party dependencies.
