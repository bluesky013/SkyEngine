## ADDED Requirements

### Requirement: Built-in fallback provider
The core SHALL provide a built-in `IUIFontProvider` that needs no external font file or plugin, so text
measurement and layout function in any build. It SHALL produce deterministic metrics so measurement is exact.

#### Scenario: Text works without a font plugin
- **WHEN** text is measured or laid out using the built-in provider
- **THEN** it SHALL produce non-zero, deterministic bounds without loading a font file

### Requirement: Text widget
The UI system SHALL provide a `Text` widget that holds content, font size, color, and alignment, measures
itself through the text system for AUTO sizing, and paints glyph quads referencing the atlas texture handle.

#### Scenario: Auto size from content
- **WHEN** a text widget with AUTO sizing is measured
- **THEN** its desired size SHALL equal the measured text bounds

#### Scenario: Paint emits glyphs
- **WHEN** a configured text widget is painted
- **THEN** it SHALL emit draw data referencing the atlas texture handle

#### Scenario: No text system configured
- **WHEN** a text widget is measured or painted without a text system
- **THEN** it SHALL report zero size and emit no draw data

### Requirement: Optional FreeType provider module
The system SHALL provide a FreeType-backed `IUIFontProvider` in a module that is only built when
`SKY_BUILD_FREETYPE` is enabled. The module SHALL load font bytes through the core file system and SHALL NOT
depend on the legacy render text stack.

#### Scenario: FreeType glyphs when enabled
- **WHEN** the module is built with `SKY_BUILD_FREETYPE` on and a valid font file is loaded
- **THEN** requesting a glyph SHALL return a non-empty coverage bitmap and positive advance

#### Scenario: Module absent when disabled
- **WHEN** `SKY_BUILD_FREETYPE` is off
- **THEN** the module and its tests SHALL NOT be built, and the built-in provider SHALL still be available
