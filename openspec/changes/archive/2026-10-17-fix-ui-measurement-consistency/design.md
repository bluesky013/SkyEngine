## Context

T06 addresses review findings UI-06 and UI-07. The constrained measurement entry is non-virtual:
MeasureDesired normalizes constraints, invokes OnMeasure, clamps the result, and conditionally
retains it. UIElement::OnMeasure adapts the separate legacy virtual Measure. Migrated native
classes override OnMeasure and implement Measure as an unbounded wrapper around MeasureDesired.

## Goals / Non-Goals

Goals are consistent supported old/new measurement dispatch, an explicit native-subclass migration,
and stable wrapped lines under representable fractional placement. Preserve pure layout queries,
hard budgets, current invalidation, and default custom-widget volatility.

This repair does not replace float positioned coordinates, introduce hidden measurement contexts
or last-query state, redesign caches, or add Unicode/word shaping.

## Decisions

### Preserve an explicit logical allocation before positioning its edges

UIElement stores arrangedSize as an allocation output. Layout resolves a child's size against an
explicit logical parentSize before forming absolute bounds, then passes both bounds and logical
size to Arrange. Slot owners pass their original size through Arrange(rect, size). Native containers
derive subsequent budgets from logical content size and preserve child allocations before adding
an absolute position. Measure remains a pure query and never assigns allocation state.

For rect-only Layout/IfNeeded, endpoint differences define the parent reference budget; Layout
still resolves the child's own Fixed/Fill/Percent/anchor rules against that budget. Rect-only
Arrange/IfNeeded and SetBounds derive their final allocation from endpoint differences and do not
recover a prior measure or Fixed request. A host which knows its pre-positioning parent/slot
width and height can pass them explicitly. Sizes are normalized to finite nonnegative values.

UILayoutParams::ContentSize deflates the logical extent with the same pure constraint operation
used in measurement. GetContentSize exposes that arranged budget; ContentRect remains positioned
geometry for paint origins, clipping, and input. Text paint uses logical content width for both
wrapping and line alignment. It never rebuilds that budget by subtracting absolute float edges.
Relative tolerance remains limited to ordinary width/glyph arithmetic and is independent of origin.

Native HBox/VBox retain each child's measured size alongside its temporary local rect; StackPanel
does likewise for slot allocations. Grid/Dock use explicit slot sizes, ScrollView preserves measured
content extent, ListView preserves viewport-width/item-height allocations, Overlay forwards its
logical content size, and the default UIElement passes that size into child Layout. Generic and
StackPanel-specific padding are deflated consistently before positioning.

Incremental arrangement keys include logical allocation size as well as positioned bounds. Layout
keys additionally include logical parentSize and the previous logical output, so two budgets which
produce identical rounded float endpoints still trigger a fresh pass. An unrelated measurement
query cannot alter a later paint or layout/arrangement skip key. Desired-size cache entries still
use normalized constraints and measurement generation.

The legacy no-argument DockPanel/GridPanel/StackPanel::Layout force-reflow wrappers preserve both
GetBounds() and GetArrangedSize(); forcing a pass must not collapse an explicit logical allocation
back into rounded endpoint differences.

The integration review also covered outer width 12.5, origin 500.1, padding 0.1/0.2 and outer width
12.3, origin -500.1, right padding 0.1. Origin-scaled epsilon or endpoint quantization cannot preserve
these fits while distinguishing a slightly narrower logical budget. The explicit allocation protocol
retains that distinction even when two resulting absolute rectangles have identical float endpoints.
This does not increase positioned geometry precision; clipping/input still use existing float bounds.

### BREAKING: one native customization hook, with an enforced migration

Measure and OnMeasure occupy independent virtual slots. An inherited native OnMeasure cannot
discover whether a more-derived class overrides legacy Measure through standard virtual dispatch.
Routing the native hook back to the wrapper creates a cycle; detecting vtable/RTTI layout or adding
a hidden constraint context introduces ABI assumptions or reentrant mutable state.

Choose a small explicit source migration instead:

- UIElement::Measure stays virtual and the default OnMeasure keeps its one-way legacy bridge.
  Custom widgets inheriting that default continue to work and remain volatile by default.
- Text, HBox, VBox, StackPanel, DockPanel, GridPanel, Overlay, ScrollView, and ListView make their
  legacy wrapper final. Their subclasses override OnMeasure. Calls through either the old public
  Measure API or MeasureDesired then reach that one derived hook without recursion.
- A former native subclass overriding Measure fails at compile time rather than returning a
  different result depending on its caller. Change its signature to
  `UISize OnMeasure(const UIConstraints &constraints) override`, return its outer desired size,
  and use the constraint-aware base OnMeasure where required. The common wrapper still clamps.
- Native subclasses inheriting retained layout must call MarkLayoutDirty when their measurement
  inputs change, or explicitly use SetLayoutVolatile(true) for unnotified external state. This is the
  existing native invalidation contract; the migration does not silently relax it.

Example migration:

```cpp
class CustomLabel : public Text {
public:
    void SetExtraWidth(float value)
    {
        if (extraWidth != value) {
            extraWidth = value;
            MarkLayoutDirty();
        }
    }

protected:
    UISize OnMeasure(const UIConstraints &constraints) override
    {
        const UISize text = Text::OnMeasure(constraints);
        return {text.width + extraWidth, text.height};
    }

private:
    float extraWidth = 0.0f;
};
```

Both legacy `label.Measure(w, h)` and `label.MeasureDesired(budget)` now use the hook; the latter
enforces its hard budget. No repository production instance of the rejected legacy native-subclass
pattern was found. This is a documented compatibility boundary, not transparent preservation of
that formerly ambiguous override.

## Risks / Trade-offs

- External native subclasses need a source migration. Final declarations and public header
  comments make the failure explicit; keeping both hooks overridable leaves silent inconsistency.
- Custom slot owners that subtract already positioned edges keep that actual endpoint budget.
  Preserve a prior logical allocation with the explicit overload when exact measure/paint agreement
  is required. Native containers are migrated together; unrelated custom hosts opt in explicitly.
- Native retained custom hooks can become stale if their owners do not notify input changes.
  Keep conservative custom defaults and test cache reuse plus MarkLayoutDirty invalidation.

## Validation Strategy

- UIMeasureTest: 175 origin/padding/width combinations assert desired height and emitted line Y,
  including close narrower controls and the integration review's additional padded cases.
- Explicit fractional slots and parent budgets with identical positioned float endpoints still
  change incremental arrangement/layout; rect-only calls restore endpoint-size semantics.
- Pure measurement preserves the prior arranged logical size. All native containers and nested
  VBox/StackPanel padding propagate fitting/narrower content budgets before absolute positioning.
- All nine native base classes: migrated subclass hooks agree through old/new entry points,
  clamp hard constraints, arrange the desired extent, reuse caches, and invalidate on notification.
- A direct UIElement legacy-only subclass remains compatible and conservatively volatile.
- Compile a former Text-only Measure override as a negative source probe and confirm a final
  override diagnostic. This is an API migration check, not a runtime ABI test.
- Root integration runs UICoreTest/UITextTest and relevant editor targets, records actual results,
  and performs strict OpenSpec validation. No native GPU check is required for this CPU repair.
