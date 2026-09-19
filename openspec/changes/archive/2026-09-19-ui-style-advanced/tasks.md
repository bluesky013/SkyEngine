## 1. Style cascade

- [x] 1.1 Add a `flags` bitmask + field setters to `UIStyle`
- [x] 1.2 Make `UITheme::Resolve` merge classes per field (with full-override fallback)

## 2. Nine-slice

- [x] 2.1 Add slice insets to `Image` and emit nine quads in `OnPaint`

## 3. Tests

- [x] 3.1 Tests: per-field merge, later class wins per field, nine quads with insets, single quad without
- [x] 3.2 Build and run the suite green
