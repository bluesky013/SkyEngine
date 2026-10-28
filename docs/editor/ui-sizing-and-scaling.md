---
title: "Editor UI Sizing & DPI Scaling"
description: "Single style source, content-derived sizing, and DPI/UI scaling for the Sandbox editor UI (ImGui-style)."
updated: "2026-10-27"
---

## Purpose

Define **one** way the Sandbox editor UI is sized and scaled, so every view (shell chrome, panels,
dialogs, hub, widgets) behaves identically at any DPI / user scale. This is the reference for new UI
code; ad-hoc pixel constants and per-view scale multipliers are **out of policy**.

The model mirrors Dear ImGui: a single `ImGuiStyle` as the source of dimensions, `style.ScaleAllSizes`
for scaling, and fonts rasterized at the physical size. View code derives sizes from the style and the
font, it does not hard-code pixels.

## Model

| Concept | ImGui | SkyEngine editor |
|---|---|---|
| Dimension source | `ImGuiStyle` | `sky::editor::UiTheme` (colors + `UiMetrics` + `UiFonts`) |
| Scale all sizes | `style.ScaleAllSizes(scale)` | `MakeDarkTheme(scale)` multiplies every metric and font size |
| Crisp text | `ImFontAtlas` rasterized at `SizePixels = base * scale` | `UIFontAtlas` keyed by `(size, codepoint)`, fonts already scaled |
| Frame height | `GetFrameHeight() = GetFontSize() + 2*FramePadding` | `metrics.frameHeight` (kept ≥ font height) |
| Current scale | `io.FontGlobalScale` | `UiTheme::scale` (= device pixel ratio × user preference) |

**Coordinate space.** The UI works in **physical pixels** at the current scale: `MakeDarkTheme(scale)`
bakes the scale into metrics + font sizes, and layout/hit-testing use the same rects, so they are always
consistent. There is **no paint-time transform** (that would blur the non-SDF font atlas).

**Effective scale.** `systemUiScale` comes from the main `NativeWindow::GetDpiScale()` before the
editor or hub is built: `GetDpiForWindow / 96` on Win32, and the window's `backingScaleFactor` on
macOS. On a Retina 2× window, a base 12-pixel font is rasterized at 24 backing pixels, preserving
its 12-point visual size. `SKY_UI_SCALE` can override the device scale for development.
`uiScale = systemUiScale * editor.uiScale` (user preference, `Preferences > Editor > UI Scale`).
`MakeDarkTheme(uiScale)` is applied once at startup, for **both** the editor and the Project Manager hub.

## Sizing rules

1. **Dimensions come from `UiTheme::metrics`.** Padding, spacing, row/frame/button heights, popup row
   height, title/footer heights, sidebar width, dialog min size — all are metrics.
2. **Typography comes from `UiTheme::fonts.`** Glyph size is `fonts.<role>`.
3. **Heights track the font.** `frameHeight` is authored as `label + 2*controlPad` (the analogue of
   `GetFrameHeight()`), so a larger font implies taller controls without extra code.
4. **Row heights are a metric** (`metrics.rowHeight`), used by list/form/nav panels.
5. **Views never write numeric pixel literals** for layout. Where a view genuinely needs a bespoke
   dimension (e.g. a dialog's nominal size), it is expressed through a metric or computed from the
   parent/content — not a magic number.
6. **No per-view scale shims.** A view must not multiply by a scale factor itself; if it does, a metric
   is missing.

## Applying a scale change

Applied at startup (before views are built). Live changes are a "retheme": drop cached panel views and
`Rebuild()` the shell; the rebuild is deferred to the next frame so it never destroys a modal mid-dispatch.

## Non-goals

- A paint-time DPI transform (blurry text with a rasterized atlas).
- Per-view scale multipliers / implicit-conversion `dp` shims.
- Per-monitor live re-scale without a rebuild.

## Migration (done)

- `UiMetrics` carries the full style (panels, spacing, controls, lists, dialogs, hub, popups) and
  `UiMetrics::Scale` multiplies every field; `MakeDarkTheme(scale)` calls it and scales `UiFonts`.
- Converted to read `metrics` / `fonts`: `WorldConfigPanel`, `ProjectManagerView` (hub),
  `FileBrowserDialog`, `NewWorldDialog`, `PreferencesDialog`, and the enum/asset widget popups.
- Removed the transitional `UiPx` / `UiFont` helpers and the hub's local `S()/FS()`. Only the low-level
  draw utility (`uidraw::Slider` / `Field`) uses `GetThemeScale()` for its primitive proportions.
- `UiFonts` gained a `banner` role for launcher/hero headings.
- The asset browser uses theme fonts and metrics for rows, thumbnails, details and hit testing.
  Its detail allocation stays stable across selection so a double-click keeps targeting the same tile.
- Menu popups measure separate check, icon, label, shortcut and arrow columns; status text uses the theme font.
- All layout literals in the converted views (hub, dialogs, config panel, widget popups) are now metrics
  or expressions of metrics; only semantic constants (e.g. dividing a color row into 4 channels) remain.

## Verification

- `EditorShellTest` keeps geometry assertions in scaled terms (they run at the default scale 1.0).
- 2× regressions cover popup column separation and shortcut-area clicks, status glyph size, asset
  tree/tile hit testing and double-clicking after a repaint, and Inspector axis label gutters.

On 2026-10-27, the existing macOS Xcode Debug build succeeded, including the editor extension
modules. `EditorShellTest` passed 148 cases and `EditorWidgetsTest` passed 42 cases, including the
six new 2× regressions. Evidence is in `cmake-build-debug/mac-ui-final-{build,shell,widgets}.log`.
Computer Use confirmed readable Retina asset/status text, separated File-menu shortcuts and
visible Inspector axis labels. The saved Cube/Plane lighting world was reopened and rendered
with Metal, then the editor exited with code 0 (`cmake-build-debug/cube-plane-final-ui.log`).

The asset browser's inline context menu and long lists still need a separate scrolling/overlay change
for very short panels; theme scaling does not add a new popup host or scrolling model.
