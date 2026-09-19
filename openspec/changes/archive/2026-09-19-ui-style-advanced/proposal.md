## Why

Theme resolution is last-class-wins, so an overlay class cannot override a single field of a base class, and
stretchable panels cannot keep fixed corners (no nine-slice). Both are render-independent style features.

## What Changes

- Make style resolution per-field: a class may set only specific fields and leave the rest to earlier classes.
- Add nine-slice rendering to `Image` so a texture's corners stay fixed while edges/center stretch.

**Non-goals**: per-widget style structs, borders/gradients, and a style authoring tool.

## Capabilities

### Modified Capabilities
- `ui-core`: per-field style cascade and nine-slice image geometry.

## Impact

- `UIStyle.{h}` (field flags + setters), `UIStyle.cpp` (`Resolve` merge), `widgets/Image.{h,cpp}` (nine-slice);
  tests in `engine/test/ui`.
