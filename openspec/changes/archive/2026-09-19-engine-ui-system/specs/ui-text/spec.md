## ADDED Requirements

### Requirement: Font registration behind a provider seam
The UI text system SHALL register fonts through an `IUIFontProvider` abstraction so the core does not depend
on a specific font rasterizer. The core SHALL ship a built-in fallback provider, and a FreeType-backed provider
SHALL be usable when `FreeTypeModule` is available, without making FreeType or the legacy render module a hard
dependency of core.

#### Scenario: Register a font
- **WHEN** a font is registered with a family name and size
- **THEN** subsequent text requests for that family and size SHALL resolve to the registered font

#### Scenario: FreeType provider when available
- **WHEN** a FreeType-backed provider is installed in a build with `FreeTypeModule` available
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
The text system SHALL measure a string and report its bounds for a given font and size, for use by layout.

#### Scenario: Measure single line
- **WHEN** a single-line string is measured
- **THEN** the reported width SHALL equal the sum of the advance widths of its glyphs and the height SHALL equal the font line height

#### Scenario: Measure multi-line
- **WHEN** a string containing a newline is measured
- **THEN** the reported height SHALL account for the number of lines

### Requirement: Text layout to draw data
The text system SHALL produce render-agnostic draw data (position, uv, color, atlas texture handle, clip rect)
for a laid-out text block, consistent with the UI core draw-data contract.

#### Scenario: Text emits quads
- **WHEN** text is laid out and painted
- **THEN** each visible glyph SHALL contribute geometry referencing the atlas texture handle

#### Scenario: Text respects clip
- **WHEN** text is laid out inside a clipped container
- **THEN** its draw commands SHALL use the container's clip rect

### Requirement: Bitmap atlas scope
This milestone SHALL support bitmap glyph atlases and Latin text. Non-Latin shaping, bi-directional text,
and signed-distance-field atlases SHALL be out of scope.

#### Scenario: Unsupported shaping is not required
- **WHEN** only Latin text is laid out in this milestone
- **THEN** no shaping or bi-directional library SHALL be a build dependency of the text system
