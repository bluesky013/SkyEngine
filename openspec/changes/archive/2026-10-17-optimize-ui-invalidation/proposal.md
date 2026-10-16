## Why

Constraint-aware measurement now makes container and scroll geometry consistent, but every layout still traverses and remeasures the whole tree. Dirty flags propagate down to descendants even when a leaf only changes its desired size, and render transforms unnecessarily invalidate layout. Incremental layout needs an explicit compatibility boundary because existing editor and custom widgets can read external state without setter notifications.

## What Changes

- Separate upward desired-size invalidation from upward arrangement-only invalidation. Geometry assigned during arrangement does not invalidate ancestors' measurement again.
- Add explicit LayoutIfNeeded/ArrangeIfNeeded entries and use the latter for UIContext layout. Within an incremental pass, existing container Layout/Arrange calls can skip clean branches with unchanged allocation inputs.
- Preserve forced behavior for existing external Layout/Arrange callers and keep UIElement layout-volatile by default. Controlled native widgets opt in; any volatile descendant keeps its ancestor path eligible for layout.
- Cache up to four normalized constraint queries only for subtrees without volatile nodes, with measurement generations that are invalidated even when dirty flags were already set.
- Preserve invalidation raised during arrangement callbacks instead of clearing it at return. Virtualized lists may need a following settling pass after live row membership changes.
- Audit native setters so content/size changes invalidate measurement, scrolling invalidates arrangement, and render transforms invalidate only paint. Localized Text remains volatile while it has a translation key.
- Continue generating a complete draw list on every paint call, including clean frames.

## Capabilities

### New Capabilities

- `ui-invalidation`: upward invalidation, opt-in incremental layout, volatile-subtree compatibility, bounded measurement caching, and invalidation preservation during callbacks.

### Modified Capabilities

- `ui-core`: replace downward dirty propagation with allocation-aware incremental layout while preserving visibility, forced host layout, and complete paint output.

## Impact

- engine/ui/core element/context implementations, native widget invalidation and volatility policies, and focused UI regression tests.
- docs/editor/ui-layout-and-chrome-plan.md records the new opt-in contract and remaining compatibility work.
- Builds on the four preceding uncommitted UI changes without modifying their artifacts, main specifications, geometry coordinates, renderer contracts, or stored document formats.
- Existing editor/custom hosts retain default volatility and explicit forced layout. Global theme/localization revisions, renderer caching, Unicode shaping, and surface ownership remain outside this change.
