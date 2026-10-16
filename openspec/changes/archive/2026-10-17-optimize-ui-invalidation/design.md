## Context

The preceding changes establish parent-owned arrangement, shared editor controls, common geometry, and constrained measurement. UIContext still calls root Arrange each frame, containers repeatedly query children, and MarkLayoutDirty recursively marks descendants. A leaf's content change does not currently mark an AUTO ancestor for measurement, while assigning a child's final bounds marks the child subtree dirty again. Dirty flags therefore neither express measurement dependencies nor safely support skipping work.

Compatibility is material: WorldConfigPanel polls its document provider in ArrangeChildren, reflected editor forms can change preferred height through external data, and localized Text resolves a process-shared table at query/paint time. These classes cannot be treated as stable merely because no setter ran.

## Goals / Non-Goals

**Goals:**

- Reuse constrained measurements and skip unchanged, controlled subtrees without changing their allocated geometry.
- Propagate desired-size changes toward consumers and propagate changed allocations downward only through actual arrangement.
- Keep legacy/custom/editor callers correct through conservative default volatility and forced existing layout entries.
- Preserve side-effect-free measurement and complete per-frame draw data.
- Make invalidation during layout callbacks observable on a subsequent pass.

**Non-Goals:**

- Geometry caching, retained draw lists, renderer/RDG/device/surface redesign, or exact polygon clipping.
- Migrating all editor polls, theme mutation, font-provider mutation, and localization changes to global revision subscriptions.
- Changing absolute layout coordinates, container allocation rules, Text wrapping/shaping, stored properties, or virtual-list item sizing.
- Automatically discovering unnotified dependencies added by subclasses of an audited native widget.

## Decisions

### Keep implementation in the runtime plugin

Dirty flags, generations, volatile counts, traversal scope, and measurement cache entries belong to UIElement/UIContext in engine/ui/core. Existing pure engine/ui constraint and geometry values remain unchanged. No engine module includes or links a plugin implementation, no process-shared registry is introduced, and no static per-DLL singleton is needed.

### Split desired-size changes from arrangement-only changes

MarkLayoutDirty marks the element and each ancestor for arrangement and paint, increments their measurement generations, clears their measurement entries, and discards their remembered Layout parent-input keys. It does not recursively dirty descendants. Content, fonts, padding/size rules, container configuration, and child membership/order changes use this conservative path.

MarkArrangeDirty marks the element and ancestors for arrangement and paint without clearing measurement entries. Scroll offsets use this path because they change the assigned content origin without changing desired size. A parent can still enter arrangement because a descendant needs work, while unaffected sibling branches remain clean.

Every mutation propagates its generation change even when a layout flag was already true. A measurement query may populate a cache while layout is dirty; a later mutation must invalidate that result rather than stopping at the existing dirty flag. Paint invalidation also travels toward ancestors rather than recursively rewriting descendants. Paint itself remains unconditional for visible content.

The public SetBounds setter marks arrangement/paint as necessary so legacy hosts can assign geometry and request descendant layout. Arrange's internal final-rectangle assignment instead updates bounds directly and marks paint when geometry changes; it does not call the public upward invalidation path or clear ancestor measurement caches. A changed allocation makes ArrangeChildren run, which passes fresh final rectangles down through the normal layout APIs. Render transforms change only paint because layout coordinates remain unchanged.

### Make incremental entries explicit and retain forced entries

Add LayoutIfNeeded(parentRect) and ArrangeIfNeeded(finalRect). UIContext::Layout calls root ArrangeIfNeeded using the root's configured bounds. UIContext's internal root is explicitly stable; attached nodes retain their own policy.

Skipping requires all of the following: no pending arrangement work, no volatile node in the branch, a previously completed compatible layout/arrangement, and exactly unchanged relevant input. Layout compares its last parent rectangle and resolved result because anchors, offsets, and percent rules depend on that reference. Arrange compares its final rectangle with the assigned bounds. Desired-size invalidation discards the old Layout parent-input key even if bounds have not changed. An explicit Arrange at the old bounds may consume current arrangement work, but must not revive that key: a following LayoutIfNeeded still resolves the new desired size. Comparing only current bounds is insufficient after a host calls SetBounds or switches entry paths.

An internal incremental traversal scope makes existing child Layout/Arrange calls made by containers participate in these checks for the duration of that pass. Nested stable branches can therefore be skipped without rewriting every allocation loop. The scope is restored on return; existing external Layout/Arrange calls continue to force their requested work and preserve host-driven polling behavior. Explicit IfNeeded calls always request the incremental contract.

A changed parent/final rectangle executes the appropriate measurement/resolution and arrangement. Children receive their newly allocated rectangles; no blanket downward invalidation pass is required. Fixed slots with unchanged final allocation can skip independently even when a different sibling causes the parent to run.

### Volatility is a conservative compatibility boundary

UIElement defaults to layout-volatile. Controlled native widgets explicitly call SetLayoutVolatile(false) after their setters and measure/arrange behavior have been audited. A subclass of an opted-in native widget inherits its policy and must explicitly restore volatility if it introduces unnotified dependencies. Hosts or direct UIElement subclasses with externally changing dependencies retain the conservative default. Opting out is a promise that relevant layout inputs are represented by constraints/rectangles and notified mutations.

Each ancestor maintains the number of volatile elements in its live subtree. Adding, removing, clearing, reparenting, or toggling policy updates that aggregate and invalidates affected layout/cache state. Detached branches retain correct local policy/counts. An incremental pass cannot skip or reuse measurements for a branch with a nonzero count, but clean stable siblings can still skip when their parent runs because another branch is volatile.

Text with a nonempty localization key remains volatile, since locale/table changes do not currently notify elements. Clearing the key restores the controlled Text policy. Existing editor views, custom legacy Measure overrides, and polling/theme-dependent hosts stay volatile and retain their explicit forced layout calls. This change does not claim to solve dependency tracking for mutable global theme or font provider state.

### Cache only stable measurement queries

MeasureDesired retains normalization, the one-way legacy bridge, and the common hard clamp. For a subtree containing no volatile node, cache up to four results keyed by the four normalized min/max width/height components and the element's measurement generation. Positive-infinite maxima remain valid keys. A bounded cache accommodates the finite-width probing used by containers without retaining unbounded constraint histories.

The common query framework does not clear layout/paint flags, assign bounds, arrange descendants, update scroll/extents, or create/remove/reorder elements on cache hits or misses. Cache storage itself is private memoization, not externally observable layout state. Measurement generation changes invalidate prior entries even when the same numerical constraints are queried again. A generation snapshot prevents storing a result across a custom callback that defensively invalidates the query's dependencies; built-in measurement hooks retain their side-effect-free contract.

Volatile branches bypass cache reads and writes. Their ancestors also bypass caching because an unchanged parent generation cannot prove that a legacy child's external state stayed the same. Measurements are not cached by raw parent pointer or current bounds; they depend on normalized constraints and notified content state.

### Preserve invalidation raised during callbacks

Arrangement consumes only the work that existed when the operation began. Track an arrangement generation and clear the corresponding dirty state only when ArrangeChildren did not raise another invalidation during the operation. Layout also snapshots that generation before MeasureForLayout starts. If a custom OnMeasure callback invalidates layout before Arrange begins, preserve the new dirty state after Arrange returns; an Arrange-only snapshot would otherwise mistake it for already-consumed work. These defensive guards do not authorize built-in measurements to mutate layout state. New invalidations remain visible for the next incremental pass; no callback-triggered work is silently discarded by a final unconditional clear.

ListView can add/remove live rows during arrangement. Those membership changes invalidate the branch, and a following pass can settle the already-created range. The implementation must not repeatedly recreate stable rows or report the branch clean while another callback requested layout. This is bounded incremental settling, not an automatic unbounded fixpoint loop.

### Audit native changes by their effect

- Text, EditBox, layout/padding/spacing/orientation/grid changes, content/child changes, and list count/item-height changes invalidate desired size.
- ScrollView and ListView offset changes invalidate arrangement without discarding reusable desired-size results.
- Setting ListView's factory clears current children/live-row metadata and uses MarkLayoutDirty, so the next arrangement rebuilds the visible range through the replacement factory. A factory supplied after the first layout also materializes rows; this membership-changing operation is deliberately stronger than an offset-only MarkArrangeDirty.
- Visual state, opacity, render transforms, and hover/pressed changes invalidate paint only.
- Setters can avoid notifications when the effective value is unchanged, but correctness does not depend on callers first clearing dirty flags.

Editor controls remain on their conservative path when their theme/reflection/provider dependencies are not fully notified. No new hand-written config UI or alternate chrome system is introduced.

### Paint remains a full output stage

UIContext::Paint begins a new UIPaintContext frame and traverses all visible content every call. Clean flags never cause a widget or subtree to disappear from the rebuilt UIDrawData. Render transforms, opacity, clips, and theme reads retain their existing behavior. The optimization is restricted to measurement/arrangement CPU work; it is not a retained render cache.

## Risks / Trade-offs

- [Unnotified external input becomes stale] -> Default volatility, aggregate volatile counts, forced existing entries, and localized Text volatility preserve legacy behavior.
- [Dirty flags hide repeated changes] -> Always increment/propagate measurement invalidation; cache population does not make a dirty branch immune to later changes.
- [Arrangement invalidates its own consumers] -> Assign final geometry internally rather than through public SetBounds.
- [A hook requests layout but the caller clears it] -> Use arrangement generations and preserve invalidation generated during callbacks.
- [Changing parents leaves stale volatile counts or measurements] -> Update old/new ancestor chains and test removal, clear, and reattachment.
- [Alternating container constraints thrash one cached value] -> Keep a small fixed capacity of four normalized query entries.
- [Frame output vanishes after a clean pass] -> Keep Paint unconditional and compare repeated complete draw output.
- [Legacy-heavy branches get limited speedup] -> Accept conservative execution until their dependency notifications are migrated deliberately; no broad timing claim is made.

## Migration Plan

1. Add runtime invalidation generations, conservative volatility/counts, bounded cache, and explicit incremental entries while retaining forced compatibility entries.
2. Move UIContext to the incremental entry; audit native widgets and their constructors/setters for opt-in and notification effects.
3. Add counter/state-based regressions for work reuse and mutation correctness, retaining the previous geometry/measurement/scroll/text/editor coverage.
4. Build relevant runtime/editor consumers, run focused suites, and validate this change strictly. Record unavailable platform/GPU checks.

No stored data migration is required. Rollback is limited to this change; the four preceding uncommitted changes remain intact. Commit and OpenSpec archive require separate explicit user confirmation.

## Validation Strategy

- Count OnMeasure/ArrangeChildren invocations across unchanged incremental passes, changed parent/final rectangles, explicit forced calls, an isolated dirty branch, and size mutation followed by explicit Arrange at old bounds before LayoutIfNeeded.
- Verify desired-size mutation reaches AUTO ancestors, keeps stable sibling branches reusable, and still invalidates caches after a query performed while dirty.
- Query normalized equivalent/different constraints, exceed the four-entry capacity, and verify generation invalidation, finite constraint results, and unchanged bounds/tree/scroll/dirty state.
- Test volatile ancestors/descendants, policy toggles, detach/clear/reattach counts, legacy external size changes, and localized Text after locale/table mutation.
- Test scrolling retains measurement reuse while updating content geometry/clamps; late/replaced factory and count changes rebuild or materialize the appropriate list rows.
- Request invalidation from custom measurement and arrangement hooks and verify it survives until a later stable pass.
- Compare complete repeated paint output after layout becomes clean, and verify transform-only changes leave layout work reusable while draw/input geometry updates.
- Run previous UI core/text/editor/widgets/shell and relevant rendering regressions, integration builds, strict OpenSpec validation, and whitespace checks without claiming unrun native GPU/Win32 validation.
