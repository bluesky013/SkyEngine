## Context

Builds on `UIElement` (clip via `clipsChildren`, `ArrangeChildren` hook) and `UIPointerEvent` wheel input.

## Decisions

### 1. ScrollView
One content child is arranged inside the view rect offset by `(scrollX, scrollY)`, then the offset is clamped
to `[0, max(0, contentSize - viewSize)]` using the child's measured size. Wheel events adjust the vertical
offset. Clipping uses the existing `clipsChildren` mechanism, so no stencil is needed.

### 2. ListView virtualization
`ListView` holds an item count, fixed item height, and an item factory. On arrange it computes the visible
index range from the scroll offset and view height, creates only the missing visible items (the rest are
destroyed), and positions each row directly. Measured height is `count * itemHeight`. This bounds the number
of live element objects regardless of list size.

## Risks / Trade-offs

- **ListView creates/destroys on scroll** -> simple and memory-bounded; a recycling pool is a follow-up.
- **No inertia/scrollbars** -> out of scope.
- **Rows are fixed height** -> variable-height rows are a follow-up.
