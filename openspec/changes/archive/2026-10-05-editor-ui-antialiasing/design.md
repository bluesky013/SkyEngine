## Context

UI drawing primitives are `UIPaintContext::AddRect` (flat) and `AddTexturedQuad` (textured). `UiDraw`
currently approximates rounded rectangles with one `AddRect` per scanline, producing hard corners. MSAA
is out of scope. Text already uses FreeType (anti-aliased) and a font atlas.

## Goals / Non-Goals

**Goals**
- Anti-aliased rounded rectangles / fields / borders without MSAA.
- Keep the existing `UiDraw` / `UiSkin` API and theme.

**Non-Goals**
- MSAA, UI shader changes (vertex format/SDF), or engine/RHI changes.

**Decisions**

### D1. 9-slice AA corner texture
Generate a small RGBA texture once: a rounded-rect corner with an anti-aliased edge and border, sized in
pixels at 1x, in white (so `AddTexturedQuad`'s vertex color tints it). Draw a rounded rect as 9 textured
quads (4 corners + 4 edges + center): corners sample the corner texture, edges/center sample a 1px
stretchable region. Fills stay flat.

**Why:** fits the existing primitives (no shader/vertex change) and gives smooth corners.
**Alternative:** SDF in the UI shader — rejected (bigger change), and MSAA — excluded.

### D2. Registry + fallback
The texture is registered through `IUITextureRegistry` (already reachable via `UITextSystem::GetRegistry()`);
if no registry is available, `RoundedRect` falls back to the current scanline approximation.

### D3. Tinting
Corners/borders are drawn in white and multiplied by the requested vertex color; borders use the
border color with the inner region filled solid.

## Risks / Trade-offs
- [Texture stretch seams] -> use exact texel-centered UVs for edges/center.
- [Many quads per rounded rect] -> 9 quads is cheaper than dozens of scanline rects.

## Migration Plan
Additive: keep the scanline path as fallback; switch `UiDraw`/`UiSkin` to the AA path when a texture
registry exists.

## As-built (what shipped)

The 9-slice texture idea was replaced by the shader approach used by UE/Blender:

- **Rounded boxes (engine/ui):** `UIVertex` carries the box center/half-extents and corner radius; a
  `shape` flag on `UIDrawCmd` selects a second pipeline whose `fs_round` fragment evaluates the rounded
  box SDF and applies screen-space (`fwidth`) anti-aliasing. `UIPaintContext::AddRoundedRect` emits these
  quads. This lives entirely in `engine/ui`, so game UI gets the same AA as the editor.
- **Text (engine/ui):** glyph quads are snapped to integer pixels (`UITextLayout::Emit`) so bitmap text
  stays crisp; the experimental SDF font path was removed (bitmap + snap is sharper at 1:1).
- **DPI:** the process declares per-monitor-v2 DPI awareness (Win32) so Windows no longer bitmap-stretches
  the window; the UI is scaled by `dpi / 96` through theme metrics/fonts (`MakeDarkTheme(scale)`), drawn
  at physical size 1:1 (crisp), not upscaled.
- **Editor independence:** the AA primitives live in `engine/ui`; the editor's `UiSkin`/`UiDraw` is only a
  style consumer.
