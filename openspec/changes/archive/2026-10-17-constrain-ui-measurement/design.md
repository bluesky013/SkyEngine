## Context

The earlier UI changes establish parent-owned Arrange, reusable editor widgets, and common affine geometry. Measure still takes only two output floats. Containers repeatedly query unconstrained intrinsic sizes, ScrollView can measure a size different from the rectangle finally assigned through Layout, and Text cannot report height for a finite width. AssetViewer custom content also receives an intrinsic query even though the right column and scrollbar determine its actual width.

This change adds a correctness contract for measurement without introducing measurement caches or changing geometry. It preserves existing absolute layout rectangles and the legacy callback surface while making finite allocation budgets explicit.

## Goals / Non-Goals

**Goals:**

- Define pure normalized min/max constraints with an unbounded positive-infinity maximum and finite desired-size results.
- Provide one enforced measurement entry and a one-way legacy bridge.
- Let width-dependent content see resolved Fixed/Fill/Percent/anchor budgets before measuring.
- Make nested containers and scroll/list allocation agree with the sizes they measured.
- Share optional byte-wise wrapped line layout between Text measurement/paint and apply it in a width-aware editor host.
- Keep measurement from mutating layout bounds, scroll offsets, element membership, or dirty flags.

**Non-Goals:**

- Geometry/culling/input changes, new coordinate systems, or renderer/shader changes.
- A cache/invalidation redesign or a promise that clean subtrees are skipped.
- Full Unicode decoding, shaping, bidi, word-breaking, or rich text.
- A new persistent layout asset schema for constraint objects or min/max widget properties.
- Surface/device/resource ownership, RDG integration, or a general editor view redesign.

## Decisions

### Pure constraint values have deterministic normalization

Add UISize and UIConstraints to engine/ui as pure data/utilities exposed through UI.Interface. UIConstraints has minimum and maximum sizes; positive infinity is a valid unbounded maximum. Normalize each axis independently:

- A negative, NaN, or infinite minimum becomes zero.
- A negative or NaN maximum becomes zero; positive infinity remains unbounded.
- If minimum exceeds maximum, lower minimum to maximum. Do not expand the hard maximum to satisfy the minimum.
- Desired negative/NaN components become zero before clamping. Positive-infinite desired size becomes the finite maximum when one exists, or the finite minimum when the maximum is unbounded. Constrain always returns finite size components.

Deflate budgets by sanitized padding, reducing available min/max extents without negative values or inf-minus-inf results. A negative or non-finite padding value contributes zero. ContentRect uses the same sanitized insets; excessive padding yields an empty rectangle inside the original bounds rather than an inverted/out-of-bounds content rectangle.

ResolveMeasureConstraints is a pure utility that translates known layout rules and a reference extent into callback constraints. Tree queries and widgets live in the separate `engine/ui/core` runtime target. `UI.Interface` does not include those runtime implementation headers; UI's explicit foundation exception permits both targets within `engine/ui`.

### Enforce budgets at one measurement entry

UIElement::MeasureDesired(constraints) is non-virtual. It normalizes the input, invokes protected OnMeasure with that budget, then hard-clamps/sanitizes the returned outer desired size. OnMeasure's return includes the element's own padding; the wrapper does not add it again. New container/Text implementations deflate padding before measuring content and add it back once.

The default OnMeasure calls legacy Measure(float&, float&) in one direction and treats its output as that subclass's outer desired size. Existing custom widgets therefore remain usable without implementing the new hook. New built-ins that make legacy Measure delegate to unbounded MeasureDesired must override OnMeasure themselves, so that the legacy bridge cannot recurse. The base legacy callback does not call the new entry.

Follow-up decision (2026-10-16), specified by `fix-ui-measurement-consistency`: this bridge preserves custom widgets inheriting the default OnMeasure. **BREAKING:** Text and migrated containers make their legacy wrapper final; their subclasses must override OnMeasure, making supported old/new queries use one hook. A legacy-only Measure override below a native class otherwise gets bypassed by inherited OnMeasure. Standard virtual dispatch cannot detect that independent override without ABI probing or a hidden mutable bridge, so the migration is enforced at compile time. Native subclasses retain their existing notification/volatility responsibilities. No production repository instance of the rejected subclass pattern was found.

MeasureDesired and MeasureForLayout are queries: they do not set bounds, arrange descendants, change scroll positions/extents, create/remove virtual rows, or mark/clear dirty flags. Internal text/glyph preparation is not a new layout cache and does not excuse layout-state mutation. No dirty-subtree or geometry cache behavior is added here.

### Resolve own rules before querying width-dependent content

MeasureForLayout is a convenience path for a child whose own UILayoutParams still apply. Resolve finite, known Fixed/Fill/Percent/anchor axes to tight callback budgets before measuring; Layout uses the same resolution before computing the final rectangle and calling Arrange. This makes a fixed-width wrapping Text see its fixed width rather than an unrelated parent width.

Ordinary Layout explicitly permits Fixed size to overflow the parent's available size. In contrast, a parent that owns the final slot calls MeasureDesired with its slot constraints and then Arrange(finalRect); the tight slot is authoritative even if the child's own Fixed rule requests a different size. Do not resolve that child rule again against an already allocated slot. This distinction separates a layout request from a parent's final decision.

Fill and Percent require a finite reference extent to become tight. If that axis is unbounded, they fall back to an intrinsic query and a finite desired result rather than propagating infinity. Anchor/offset resolution follows the existing layout convention; size measurement does not introduce parent-local origins or change render geometry.

Alternative: add optional available width to only Text. That leaves container/scroll allocation with divergent rules and makes width-dependent content unreliable under nesting. Alternative: let every virtual Measure normalize/clamp independently. That makes hard constraints unenforceable for legacy consumers.

### Pass constraints through containers and preserve slot ownership

HBox/VBox, StackPanel, DockPanel, GridPanel, and Overlay implement OnMeasure and propagate content budgets after padding. Fixed/Auto children consume their declared/measured share; Fill receives the nonnegative remaining budget after siblings, margins, gaps, and padding. Ordinary linear boxes use child layout rules through the rule-aware path. Their arrangement cursor advances by allocated extents, so Fill does not overlap preceding content.

Slot-based containers instead own their allocated child rectangles. They measure children against their slot budget and call Arrange directly, avoiding a second child Layout rule resolution. Stack weights divide remaining Fill space; Dock uses its progressively remaining content rectangle; Grid passes per-cell budgets; Overlay measures its overlapping content under the common available rectangle. Each container includes its own padding once in its outer desired size.

Explicit Fixed overflow remains an ordinary-layout behavior; negative remaining space produces zero Fill allocation rather than negative child sizes. Constraint propagation does not add new alignment or slot min/max fields in this change.

### Separate viewport constraints from scroll extent

Introduce ScrollAxes Both, Horizontal, Vertical, and None; Both remains the default. During measurement, allow an unbounded maximum on each scrolling axis so AUTO content can report its full extent. On non-scrolling axes, bound content to the viewport. During arrangement, use the actual sanitized content rectangle as the viewport.

Choose an AUTO viewport in two stages rather than repeatedly applying a child's Percent rule to a shrinking viewport. First measure against the caller's available inner budget and clamp the resulting outer size to obtain a candidate viewport. For Vertical, Both, and None, freeze that candidate width, subtract padding, then remeasure dependent height at the actual inner width. For Horizontal, freeze the candidate height and remeasure width at the actual inner height. The other axis retains its original available reference during this second query; common MeasureDesired clamping still applies to the final outer result.

Resolve content Fixed/Percent/Fill rules once using that viewport as their reference, even on an axis that scrolls. AUTO can remain unbounded on a scrolling axis; a known Fill/Percent axis uses its viewport-based resolved size. The non-scrolling viewport maximum remains a hard limit after resolution, including a child Fixed size larger than that axis. The final measurement and Arrange rectangle agree, and the stored content extent equals the final allocated size rather than an earlier intrinsic result.

ScrollView owns the final content origin: Arrange the content once at `view.left - scrollX` / `view.top - scrollY`; do not call Layout again on the already resolved rectangle. Content anchors/offsets participate only in the once-resolved extent budget and are not applied again as placement offsets. This is an explicit parent-owned slot behavior, not a promise to preserve the old content.Layout positioning side effects. Clamp each enabled offset to max(0, contentExtent - viewportExtent), and disable scrolling on the other axes. Vertical wheel behavior remains available when vertical scrolling is enabled. Measurement itself leaves offsets and previously arranged extent unchanged.

OnChildrenChanged clears a removed/cleared content pointer, so inherited child removal cannot leave SetContent's cached pointer dangling. This is local content ownership cleanup, not a general surface lifecycle change.

Alternative: keep all content axes unbounded and clip afterward. That prevents finite-width wrapping in a vertical scroller. Alternative: measure using one size and then call child Layout again. That reproduces the old extent/arrangement mismatch.

### Keep virtual-list measurement independent of live items

ListView reports its full fixed-item content extent from item count/height, subject to the common query budget, without invoking the row factory or changing live item membership/scroll state. Only arrangement chooses the visible range and creates/removes rows. Each visible row receives its final item rectangle through Arrange so a child layout rule cannot shrink or move a parent-owned row slot a second time.

This keeps virtual measurement cheap and independent of which rows happened to be visible. It does not add variable-height virtualization.

### Build text lines once per query with one shared algorithm

Text soft wrap is optional and false by default. For a finite content width, the shared line-layout builder inserts a soft break before a byte whose glyph advance would overflow a nonempty line, accounting for relative floating-point rounding tolerance; explicit newlines remain hard breaks. Width-relative tolerance alone cannot cover cancellation from a larger absolute origin. The `fix-ui-measurement-consistency` follow-up preserves an explicit logical allocation through Layout/Arrange and native containers, so Text paint deflates the same logical content budget rather than subtracting positioned edges. It consumes at least one byte when a glyph is wider than the available line, avoiding an endless zero-width loop. Unbounded width or wrap=false preserves the existing no-soft-wrap behavior.

Text.OnMeasure deflates its padding, builds line extents using the supplied content width, and adds padding back to its outer desired size before common hard clamping. After the follow-up, paint uses the same builder with the arranged logical content width; ContentRect supplies the positioned origin, and each line aligns independently for Left/Center/Right. The follow-up covers the 6.1 advance / 12.2 width / 500.1 origin review reproduction plus fractional padding and close narrower budgets, asserting measured height and emitted glyph geometry. Fixed or parent-slot width is known before line layout. Rect-only allocation entries still use actual endpoint differences; hosts preserving a measured logical slot pass that size explicitly.

Keep existing UiDraw/UITextLayout no-wrap entry points compatible. Do not silently make all editor labels wrap. Add boolean text.wrap through Text.SetProperty. UIDocument currently decodes only layout, visual, and button groups, so add generic recursive dispatch for the remaining props groups: flatten nested object keys into dotted paths and pass boolean, int64-compatible integer, other numeric float, and string leaves through UIElement.SetProperty. This also exposes the registered scroll.axes property without widget-specific casts. Keep layout/visual/button on their existing specialized paths and skip those groups during generic dispatch to preserve array, asset-reference, and color decoding without duplicate setter calls. Arrays, null, and unsupported/unknown paths remain silently ignored. JSON-built and C++-configured widgets therefore use the same setters. This is byte-wise glyph layout consistent with the existing Latin/bitmap scope, not Unicode shaping or language-aware word wrapping.

### Measure editor custom content at its actual width

AssetViewer measures custom content with the width of the column it will receive after padding and scrollbar reservation. If the scrollbar changes that width, the height used for scrolling must correspond to the resulting actual width. Allocate the resulting final rectangle through Arrange, leaving Measure as a query. This is a concrete consumer of the new API and does not migrate unrelated editor panels.

## Risks / Trade-offs

- [Legacy/new Measure callbacks recurse] -> Keep the default bridge one-way and require new delegating built-ins to override OnMeasure; test both kinds.
- [Invalid or contradictory constraints leak NaN/infinity] -> Normalize all entry budgets, lower contradictory minima, and hard-sanitize desired output in the non-virtual wrapper.
- [Fixed overflow conflicts with hard slots] -> Distinguish own-rule MeasureForLayout from parent-owned MeasureDesired/Arrange and test both paths.
- [Padding is counted twice or inverts content bounds] -> Define outer desired once, sanitize insets, and test excessive padding and tight zero-size budgets.
- [Scroll extent differs from final content rectangle] -> Resolve content rules against the viewport once, enforce non-scroll-axis maxima, and compare extent/rectangle/clamp together.
- [Wrapped text paints a different set of lines] -> Share the line builder, use the same effective content width, and test per-line alignment and explicit newline handling.
- [Measurement creates rows or changes interactive state] -> Assert bounds/children/scroll/dirty state remains unchanged across repeated queries.
- [Floating-point and byte layout limits remain] -> Keep finite-result normalization explicit and document that shaping, word breaks, caches, and geometry redesign are outside scope.

## Migration Plan

1. Preserve the preceding uncommitted UI changes and introduce pure constraint utilities.
2. Add the common element measurement bridge and rule-aware layout path, then migrate containers.
3. Migrate ScrollView/ListView and Text using their own constrained hooks; retain compatible no-wrap/legacy calls.
4. Add the generic wrap property and width-aware AssetViewer query; update the layout/chrome plan locally.
5. Run focused pure/runtime/text/data/editor regressions, relevant integration builds, and strict OpenSpec validation. Record unavailable platform checks explicitly.

No stored data migration is required: absent text.wrap retains false, and constraint values are runtime query inputs. Roll back this measurement change together without reverting the preceding geometry/editor changes. Commit/archive still require user confirmation.

## Validation Strategy

- Pure normalization/deflation/constrain tests covering zero, negative, NaN, infinities, contradictory min/max, and padding saturation.
- Legacy bridge and new-hook tests verifying callback budgets, finite hard clamps, no recursion, Fixed overflow versus tight-slot authority, and unbounded Fill/Percent fallback.
- Nested container tests with width-dependent child probes, weighted/remaining Fill, padding/gaps, and final rects matching queried slot budgets.
- All four scroll axes, viewport-based own-rule resolution, extent/rect/clamp agreement, content removal safety, and measurement state snapshots.
- ListView factory-call/live-item assertions before and after constrained queries and visible-range arrangement.
- Wrapped Text line-count/height/per-line alignment tests, no-wrap API compatibility, JSON/C++ property parity including actual glyph positions, generic nested scalar type dispatch and unchanged special-group decoding, and actual-width AssetViewer integration.
- Applicable UI/editor tests/builds plus strict OpenSpec validation; no cache/Geometry/shaping completion claim.

## Open Questions

- No question blocks this scope. Full invalidation/caching, Unicode shaping, and broader editor layout migration remain later designs.
