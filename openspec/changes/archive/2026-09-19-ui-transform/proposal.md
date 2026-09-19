## Why

Elements have no transform or opacity, so UI cannot rotate/scale, and a container cannot fade its subtree.
Both are render-independent (they change emitted vertices/colors) and belong to the core.

## What Changes

- Add a 2D transform (translation, rotation, scale, pivot) to `UIElement`.
- Apply the accumulated transform to vertices during paint via a transform stack in `UIPaintContext`.
- Add element opacity and multiply it into emitted vertex alpha (hierarchical cascade).

**Non-goals**: clipping under rotation (scissor stays axis-aligned), 3D transforms, and skew.

## Capabilities

### Modified Capabilities
- `ui-core`: add element transforms and hierarchical opacity to draw-data emission.

## Impact

- `UIRect.h` (`UI2DTransform`), `UIPaintContext.{h,cpp}`, `UIElement.{h,cpp}`, new `UITransform.h`; tests in
  `engine/test/ui`.
