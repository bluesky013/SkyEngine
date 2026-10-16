# ui-text Specification

## Purpose
TBD - created by archiving change engine-ui-system. Update Purpose after archive.
## Requirements
### Requirement: Font registration behind a provider seam
The UI text system SHALL register fonts through an `IUIFontProvider` abstraction exposed by `UI.Interface`, so the core does not depend on a specific font rasterizer. The core SHALL ship a built-in fallback provider, and a FreeType-backed provider SHALL be usable through the optional `UI.FreeType` adaptor when `SKY_BUILD_FREETYPE` is enabled, without making FreeType or the legacy render module a hard dependency of core.

#### Scenario: Register a font
- **WHEN** a font is registered with a family name and size
- **THEN** subsequent text requests for that family and size SHALL resolve to the registered font

#### Scenario: FreeType provider when available
- **WHEN** a FreeType-backed provider from `UI.FreeType` is installed in a build with `SKY_BUILD_FREETYPE` enabled
- **THEN** glyph rasterization SHALL go through that provider

#### Scenario: Core works without FreeType
- **WHEN** the UI core is built with `SKY_BUILD_FREETYPE` disabled
- **THEN** text SHALL still function using the built-in fallback provider

### Requirement: Glyph atlas pages
The text system SHALL rasterize glyphs into atlas pages and register each atlas page with the UI texture
registry, returning a stable texture handle for use in draw commands.

#### Scenario: New glyph added to atlas
- **WHEN** a glyph not yet present is requested
- **THEN** it SHALL be rasterized and packed into an atlas page, and the atlas page SHALL have a registered texture handle

#### Scenario: Atlas page reused
- **WHEN** the same glyph is requested again
- **THEN** the cached atlas entry SHALL be reused without re-rasterizing

#### Scenario: Atlas growth
- **WHEN** the active atlas page has no room for a new glyph
- **THEN** a new atlas page SHALL be created and registered, and existing handles SHALL remain valid

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

### Requirement: Bitmap atlas scope
This milestone SHALL support bitmap glyph atlases and Latin text. Non-Latin shaping, bi-directional text,
and signed-distance-field atlases SHALL be out of scope.

#### Scenario: Unsupported shaping is not required
- **WHEN** only Latin text is laid out in this milestone
- **THEN** no shaping or bi-directional library SHALL be a build dependency of the text system

### Requirement: Built-in fallback provider
The core SHALL provide a built-in `IUIFontProvider` that needs no external font file or plugin, so text
measurement and layout function in any build. It SHALL produce deterministic metrics so measurement is exact.

#### Scenario: Text works without a font plugin
- **WHEN** text is measured or laid out using the built-in provider
- **THEN** it SHALL produce non-zero, deterministic bounds without loading a font file

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

### Requirement: Optional FreeType provider
The UI foundation SHALL provide a FreeType-backed `IUIFontProvider` in `engine/ui/freetype` as target `UI.FreeType`, built only when `SKY_BUILD_FREETYPE` is enabled. Provider callers SHALL explicitly link that target; the `UI` runtime core SHALL NOT link FreeType or the adaptor. The provider SHALL load font bytes through the core file system and SHALL NOT depend on the legacy render text stack. The existing `ui/freetype/...` include paths SHALL remain available from `UI.FreeType`.

#### Scenario: FreeType glyphs when enabled
- **WHEN** a caller links `UI.FreeType` in a build with `SKY_BUILD_FREETYPE` on and a valid font file is loaded
- **THEN** requesting a glyph SHALL return a non-empty coverage bitmap and positive advance

#### Scenario: Provider absent when disabled
- **WHEN** `SKY_BUILD_FREETYPE` is off
- **THEN** the provider and its tests SHALL NOT be built, and the built-in provider SHALL still be available

#### Scenario: Core stays independent when the adaptor is enabled
- **WHEN** `SKY_BUILD_FREETYPE` is on and a caller links only `UI`
- **THEN** the caller SHALL be able to use the fallback text system without acquiring a `UI.FreeType` or FreeType dependency through `UI`

### Requirement: Borrowed atlas registry lifetime

UIFontAtlas and UITextSystem SHALL support paired attachment and detachment of borrowed texture registries. The owner SHALL detach a registry before destroying it. Detachment SHALL stop all future glyph update and page registration calls to that registry without invoking it or releasing its textures. Duplicate attachment and unknown/null detachment SHALL be harmless.

#### Scenario: Detached renderer is destroyed
- **WHEN** a secondary atlas registry is detached and destroyed and new glyphs or pages are subsequently requested
- **THEN** only still-attached registries SHALL receive page registrations and updates
- **AND** cached glyph/page handles SHALL remain valid

#### Scenario: Registry is reattached
- **WHEN** a previously detached registry is attached again
- **THEN** all current pages SHALL be replayed under their existing ids exactly once
- **AND** a retained backend page SHALL receive current atlas pixels before cached glyphs are used
- **AND** duplicate attachment SHALL NOT replay pages or duplicate future update calls

#### Scenario: Default registry is removed
- **WHEN** UITextSystem's default registry is detached
- **THEN** GetRegistry SHALL return null until another registry is explicitly attached
- **AND** removing a secondary registry SHALL preserve an existing default

#### Scenario: Editor surface cleanup
- **WHEN** the editor explicitly destroys a floating surface, processes its OS-close, or shuts down all surfaces
- **THEN** each renderer SHALL be detached from the text system before that renderer is destroyed
