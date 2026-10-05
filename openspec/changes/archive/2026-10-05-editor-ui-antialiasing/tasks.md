## 1. Anti-aliased rounded boxes (engine/ui, editor-independent)

- [x] 1.1 Add rounded-box SDF parameters to `UIVertex` (center/half/radius) and a `shape` flag to
      `UIDrawCmd`; add `UIPaintContext::AddRoundedRect`.
- [x] 1.2 Add the `fs_round` fragment (analytic rounded-box SDF AA) plus a second pipeline, selected
      per command; fall back to the flat pipeline if it is unavailable.
- [x] 1.3 Route `UiDraw::RoundedRect` (editor style layer) through `AddRoundedRect` so all skin
      components get the same AA.

## 2. Crisp text (engine/ui, editor-independent)

- [x] 2.1 Snap glyph quads to integer pixels in `UITextLayout::Emit`.
- [x] 2.2 Remove the experimental SDF font path (bitmap + pixel-snap is sharper at 1:1).

## 3. DPI

- [x] 3.1 Declare per-monitor-v2 DPI awareness in `Win32Window` before creating the window.
- [x] 3.2 Scale theme metrics/fonts by `dpi / 96` (`MakeDarkTheme(scale)`); the module reads
      `GetDpiForWindow` and calls `shell.SetUiScale`; `SKY_UI_SCALE` overrides for testing.

## 4. Build and verification

- [x] 4.1 Build `SandboxEditor`; rounded corners are smooth, text is crisp, and the UI scales with the
      monitor DPI.
