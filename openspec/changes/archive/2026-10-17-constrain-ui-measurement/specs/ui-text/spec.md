## MODIFIED Requirements

### Requirement: Text measurement
The text system SHALL measure a string and report its bounds for a given font/size using a line-layout algorithm shared with emission. Existing no-wrap calls SHALL preserve explicit-newline and glyph-advance behavior. An optional soft-wrap query SHALL use finite available content width to insert byte-wise breaks, preserve explicit newlines, and make progress even when one glyph is wider than the line. Unbounded width SHALL not cause soft wrapping. Full Unicode shaping and language-aware word breaking SHALL remain out of scope.

#### Scenario: Measure single line
- **WHEN** a single-line string is measured without soft wrapping
- **THEN** its width SHALL equal the sum of glyph advances and its height SHALL equal the font line height

#### Scenario: Measure multi-line
- **WHEN** a string containing a newline is measured
- **THEN** its height SHALL account for the explicit lines

#### Scenario: Finite width adds soft lines
- **WHEN** soft wrapping is enabled and the next byte's glyph would overflow a nonempty finite-width line, accounting for relative floating-point rounding tolerance
- **THEN** the shared layout SHALL begin a new line before that byte and include the new line in its height

#### Scenario: Narrow width still consumes input
- **WHEN** one glyph is wider than the available wrap width
- **THEN** layout SHALL consume that byte rather than repeatedly producing an empty line

### Requirement: Text layout to draw data
The text system SHALL produce render-agnostic glyph draw data containing position, uv, color, atlas texture handle, and clip rect, consistent with UI core. Text measurement and emission SHALL use the same line-layout algorithm and content width. Each line SHALL be aligned independently within the content rectangle; ordinary UiDraw/no-wrap entry points SHALL preserve their previous behavior.

#### Scenario: Text emits quads
- **WHEN** text is laid out and painted
- **THEN** each visible glyph SHALL contribute geometry referencing its atlas texture handle

#### Scenario: Text respects clip
- **WHEN** text is laid out inside a clipped container
- **THEN** its draw commands SHALL use the container's clip rect

#### Scenario: Measured and painted wrapped lines agree
- **WHEN** a Text is measured and painted with the same finite content width and wrap setting
- **THEN** emitted line breaks and layout height SHALL correspond to the measured line layout

#### Scenario: Fractional placement preserves an exactly fitting line
- **WHEN** glyph advances exactly fit the logical wrap width and subtraction of fractional arranged edges introduces only floating-point rounding error
- **THEN** painting SHALL preserve the measured line break, while a meaningful reduction in available width SHALL still produce a soft break

#### Scenario: Each line aligns independently
- **WHEN** multiline Text uses Center or Right alignment
- **THEN** each line SHALL be positioned from its own measured width within the padded content rectangle

### Requirement: Text widget
The UI system SHALL provide a Text widget holding content, font size, color, alignment, and a default-false wrap flag. Its OnMeasure SHALL use the supplied budget after deflating own padding, then return outer desired size including padding for common hard clamping. Paint SHALL use the actual padded content width with the same line-layout rules. Migrated legacy Measure SHALL delegate through its own OnMeasure implementation without recursion, and a missing text system SHALL produce no glyph draw data.

#### Scenario: Auto size from content
- **WHEN** an unconstrained AUTO Text is measured with zero padding
- **THEN** its desired size SHALL equal the measured text bounds

#### Scenario: Paint emits glyphs
- **WHEN** a configured Text is painted
- **THEN** it SHALL emit draw data referencing its atlas texture handle

#### Scenario: No text system configured
- **WHEN** an unconstrained zero-padding Text is measured or painted without a text system
- **THEN** it SHALL report zero content size and emit no glyph draw data

#### Scenario: Padding is included once
- **WHEN** a wrapping Text is measured under finite outer width with padding
- **THEN** its line layout SHALL use the deflated content width and its outer desired size SHALL include padding once before hard clamping

#### Scenario: Existing Text does not wrap implicitly
- **WHEN** no wrap flag is supplied
- **THEN** Text SHALL retain no-soft-wrap behavior
