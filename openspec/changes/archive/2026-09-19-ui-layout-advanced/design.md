## Context

Builds on `engine/ui/core` (`UILayoutParams`, `UIElement::Measure/Layout`, `UIPaintContext`,
`UIEventRouter`). Children currently paint/hit-test in insertion order.

## Decisions

### 1. Percent sizing
Add `UISizeMode::PERCENT`; `ResolveExtent` returns `parentExtent * value` where `value` is a fraction. The
document loader parses `"50%"` to `0.5` and `"fill"`/`"auto"` as before.

### 2. Z-order
Add `float z` to `UILayoutParams`. `UIElement::GetPaintOrder()` returns children sorted by `z` with a stable
sort, so equal-z children keep insertion order. Paint walks paint order; hit-testing walks it in reverse.

### 3. Linear containers
Add `virtual void ArrangeChildren(const UIRect &content)` called by `UIElement::Layout` after bounds are
resolved (base implementation arranges every child against `content`). `HBox`/`VBox` override it to place
children sequentially with spacing; their `Measure` sums child sizes (max cross-axis) plus padding/spacing.
Alternative: a layout-manager component per container — rejected as heavier than needed now.

## Risks / Trade-offs

- **Percent of parent with AUTO self size** can be circular; percent is only resolved when a parent extent is
  known (stretch/FILL/parent size), otherwise it falls back to 0.
- **FILL children in a linear container** currently take their measured size, not a share of the free space;
  flexible grow/shrink is a follow-up.
