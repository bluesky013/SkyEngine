## MODIFIED Requirements

### Requirement: Text measurement
The text system SHALL measure a string and report its bounds for a given font/size using a line-layout algorithm shared with emission. Existing no-wrap calls SHALL preserve explicit-newline and glyph-advance behavior. An optional soft-wrap query SHALL use finite available logical content width to insert byte-wise breaks, preserve explicit newlines, and make progress even when one glyph is wider than the line. Text paint SHALL use the arranged logical allocation deflated by the same pure padding operation as measurement, without reconstructing its budget from positioned float edges. Overflow comparison MAY allow small width-relative arithmetic rounding but SHALL NOT expand tolerance based on absolute origin. A zero budget SHALL remain zero and a narrower logical width beyond arithmetic tolerance SHALL still wrap, even if its positioned endpoints round to the same coordinates as a fitting allocation. Unbounded width SHALL not cause soft wrapping. Full Unicode shaping and language-aware word breaking SHALL remain out of scope.

#### Scenario: Measure single line
- **WHEN** a single-line string is measured without soft wrapping
- **THEN** its width SHALL equal the sum of glyph advances and its height SHALL equal the font line height

#### Scenario: Measure multi-line
- **WHEN** a string containing a newline is measured
- **THEN** its height SHALL account for the explicit lines

#### Scenario: Finite width adds soft lines
- **WHEN** soft wrapping is enabled and the next glyph overflows a nonempty finite-width line beyond width-relative arithmetic rounding
- **THEN** the shared layout SHALL begin a new line before that byte and include the new line in its height

#### Scenario: Fractional origin preserves an exact fitting line
- **WHEN** two glyph advances of 6.1 fit a logical width of 12.2 and fractional placement at origin 500.1 reduces the width obtained by subtracting float edges
- **THEN** Text painting SHALL use its logical content allocation and keep both glyphs on the measured line

#### Scenario: Fractional padding preserves an exact fitting line
- **WHEN** Fixed outer width 12.5, origin 500.1, padding left 0.1 and right 0.2 produce a fitting logical content width but a smaller positioned ContentRect width
- **THEN** Text SHALL preserve the measured line through its explicitly retained logical allocation

#### Scenario: Meaningfully narrower width still wraps
- **WHEN** the same glyphs are measured and painted with a content width of 12.19998 or 12.1 at positive or negative fractional origins
- **THEN** the second glyph SHALL start a new line in both desired height and emitted geometry

#### Scenario: Zero width still consumes input
- **WHEN** positive-advance glyphs are wrapped with zero available content width
- **THEN** each byte SHALL be consumed and each oversized glyph SHALL occupy its own line regardless of origin
