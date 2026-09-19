## Context

Builds on `UIPaintContext` (clip stack, batched quads) and `UIElement::Paint`.

## Decisions

### 1. 2D transform
`UITransform` holds translation, rotation (radians), non-uniform scale, and a pivot; `ToMatrix()` composes
`T(translation) * T(pivot) * R * S * T(-pivot)` into a `UI2DTransform` (2x3 affine). `UIPaintContext` keeps a
transform stack; `PushTransform(local)` multiplies the current matrix, `PopTransform` restores. `AddQuad`
transforms each vertex position. Clipping stays axis-aligned scissor (rotation-aware clipping is a non-goal).

### 2. Opacity cascade
`UIElement::opacity` (0..1). `UIPaintContext` keeps an opacity stack; `PushOpacity` multiplies the current
value, and `AddQuad` scales each vertex's alpha. `UIElement::Paint` pushes its transform/opacity around
`OnPaint` + children and pops them after, so a faded container fades its subtree.

## Risks / Trade-offs

- **Rotated content still clips by axis-aligned bounds** -> documented; shader-side clip is a follow-up.
- **Batched commands unaffected** -> vertices are transformed before batching; textures/uv unchanged.
