## MODIFIED Requirements

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
