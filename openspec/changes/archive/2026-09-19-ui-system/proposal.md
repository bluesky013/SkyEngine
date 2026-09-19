## Why

UI has no DPI scaling or localization, so high-density displays and non-English titles cannot be handled. Both
are render-independent.

## What Changes

- Add a DPI scale to `UIContext` that scales the logical content size into device coordinates.
- Add `UILocalization`: per-locale string tables, a current locale, and key translation with key fallback.
- Let `Text` reference a localization key and resolve it at measure/paint time.

**Non-goals**: full locale negotiation, plural/gender rules, and DPI-aware glyph rasterization (font provider).

## Capabilities

### New Capabilities
- `ui-system`: DPI scaling and localization for the UI layer.

## Impact

- `UIContext.{h,cpp}` (DPI scale), new `localization/UILocalization.{h,cpp}`, `widgets/Text.{h,cpp}` (text
  keys); tests in `engine/test/ui`.
