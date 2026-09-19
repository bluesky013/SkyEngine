## Context

Builds on `UIStyle`/`UITheme::Resolve` and the `Image` widget.

## Decisions

### 1. Per-field cascade
`UIStyle` gains a `flags` bitmask and field setters (`SetBackgroundColor`, `SetBorderWidth`, ...). `Resolve`
merges classes in order: a flagged field overrides the accumulator; a style with `flags == 0` is treated as a
full override (keeps direct field assignment working). Alternative: `std::optional` per field — heavier and
noisier at call sites.

### 2. Nine-slice
`Image` gains per-edge slice insets. When any inset is non-zero it emits nine quads (four corners fixed,
edges/center stretched) using sub-rects/sub-UVs; otherwise it emits the single quad as before. UV insets are
derived from the destination rect ratio (texture size is unknown at this layer).

## Risks / Trade-offs

- **UV insets derived from the rect ratio** are approximate without the source texture dimensions; exact
  insets need texture size metadata (follow-up).
- **`flags == 0` full-override fallback** keeps legacy behavior but means a class cannot be an intentional
  no-op; acceptable.
