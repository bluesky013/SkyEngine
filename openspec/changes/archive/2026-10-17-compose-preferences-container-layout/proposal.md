## Why

Preferences already uses shared reflected fields, session-owned working values, and local history. Its surrounding categories, page host, and footer still calculate separate rectangles in layout, paint, and input. Fixed minimums and footer offsets bypass UiTheme, categories do not scroll independently, and resizing or changing scale can produce geometry that differs between first layout and first paint. T11-B should finish the composition without reopening the delivered session work.

## What Changes

- Compose the Preferences body, category navigation, reflected page, and footer through shared UI containers with theme-derived dimensions and explicit logical allocation sizes.
- Give categories a bounded vertical scrolling viewport independent of the selected reflected form's existing scrolling.
- Draw its themed scrollbar inside the explicitly reserved track and preserve form capture by pointer identity, including cancellation.
- Make container-arranged bounds the source for paint, clipping, input, and existing layout-query helpers. Prepare the tree before first paint; painting does not repair or create the layout.
- Bound the dialog to available space, adapt its footer/category allocation for smaller windows, and clip long labels within their allocated controls.
- Synchronize page registration, reflected form, and default-theme dependencies before allocation, preserving safe deferred transitions and first-frame correctness after resize/reopen/scale changes.
- Retain PreferencesSession ownership/history, Apply/OK/Cancel/Reset, callback lifetime, generic reflected controls, and the existing host persistence callback.

## Capabilities

### New Capabilities

None. This is the T11-B composition of existing editor/UI capabilities.

### Modified Capabilities

- `editor-preferences`: shared-container composition, independent navigation/page scrolling, and consistent bounded layout/paint/input geometry.

## Impact

- PreferencesDialog composition and its UI/theme integration, plus focused portable layout/input and existing session regressions.
- Small editor-only skin adapters and the shared UiSkin reserved-track scrollbar contract may be adjusted where needed; runtime UI remains independent of editor settings and rendering backends.
- No settings schema, session/history baseline, generic per-page persistence/application protocol, or host/module wiring change.
- RDG, RHI, renderer/surface/GPU lifetime, Aurora adaptors, SandboxModule, and Core Event are outside scope. Windows/native display/GPU acceptance remains pending. No commit or archive is authorized.
