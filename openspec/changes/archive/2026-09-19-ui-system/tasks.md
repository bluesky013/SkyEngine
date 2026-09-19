## 1. DPI

- [x] 1.1 Add logical content size + DPI scale to `UIContext` and apply it to the root bounds

## 2. Localization

- [x] 2.1 Add `localization/UILocalization.{h,cpp}` (singleton, locale tables, `Translate` with fallback)
- [x] 2.2 Add text keys to `Text` and resolve them in measure/paint

## 3. Tests

- [x] 3.1 Tests: DPI scale applies/resets, translation in locale, missing-key fallback, text key resolution
      and locale change
- [x] 3.2 Build and run the suite green
