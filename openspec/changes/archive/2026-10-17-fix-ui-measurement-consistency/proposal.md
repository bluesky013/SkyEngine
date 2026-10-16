## Why

The UI review reproduced a wrapped Text that measures one line and paints two: glyph advances 6.1,
width 12.2, and absolute origin 500.1. Subtracting its arranged float edges produces a width of
12.19998169, exceeding the existing width-relative rounding allowance. The review also found that
a legacy-only Measure override below Text or a migrated container is silently bypassed by its
inherited OnMeasure implementation.

## What Changes

- Preserve the logical allocation size before positioning float edges, pass it explicitly through
  native containers, and make Text paint use the same deflated content budget as measurement.
- Keep zero/narrow budgets meaningful and cover measured height plus emitted glyph positions.
- **BREAKING:** make migrated native widgets' legacy Measure wrappers final. Their subclasses must
  override OnMeasure; this gives both old and constrained query entry points one dispatch path.
- Preserve the default UIElement legacy bridge for custom widgets inheriting its OnMeasure, the
  common hard clamp, conservative custom volatility, and notified native measurement caches.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `ui-text`: positioned float-edge rounding must preserve fitting wrapped lines.
- `ui-measurement`: explicitly delimit legacy bridge compatibility and enforce the native subclass
  migration at compile time instead of silently reporting two different desired sizes.

## Impact

Pure UI.Interface allocation utilities, UI runtime allocation/container/text code, public native
widget measurement declarations, and UIMeasureTest.
There is no renderer/backend dependency or stored asset/schema migration. No production repository
subclass overriding legacy Measure below a migrated native class was found. External native
subclasses using that pattern require a source migration. Unicode shaping, geometry redesign, and
new caches remain outside this repair. Existing rect-only allocation calls retain endpoint-size
semantics; slot owners that already measured a precise size can preserve it through the new overload.
