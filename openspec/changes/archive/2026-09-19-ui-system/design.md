## Context

Builds on `UIContext` (content size/layout) and the `Text` widget.

## Decisions

### 1. DPI scale
`UIContext` stores a logical content size and a DPI scale; the root bounds become `logical * scale`, so
percentage/FILL layout and painting scale together. `SetContentSize` stores the logical size; `SetDpiScale`
re-applies. Alternative: a paint-time root transform — deferred because hit-testing would also need inverse
scaling.

### 2. Localization
`UILocalization` is a `Singleton` (shared through `Environment`) with `locale -> key -> value` tables. It has a
current locale and `Translate(key)` returning the value or the key when missing, so untranslated keys remain
visible rather than blank.

### 3. Text keys
`Text` can reference a localization key (in addition to literal content). Measure/Paint resolve the key through
`UILocalization` each time, so switching locale re-localizes without rebuilding the tree. Alternative:
pre-resolve on load — rejected (locale changes would need a reload).

## Risks / Trade-offs

- **DPI scaling is layout-level, not per-glyph** -> fonts rasterize at one size; high-DPI crispness needs the
  font provider to know the scale (follow-up).
- **No pluralization/formatting** -> key/value only.
