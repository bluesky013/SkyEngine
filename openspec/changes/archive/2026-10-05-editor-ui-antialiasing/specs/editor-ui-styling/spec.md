## ADDED Requirements

### Requirement: Themeable editor styling

The editor UI SHALL be drawn through a themeable style layer where colors, metrics and fonts come from a
swappable theme, and panels/components SHALL draw through shared skin primitives rather than panel-local
layout code.

#### Scenario: One theme drives all panels

- **WHEN** a theme is supplied
- **THEN** panels SHALL render with that theme's colors, metrics and fonts

### Requirement: Anti-aliased rounded shapes without MSAA

The style layer SHALL render rounded rectangles, fields and borders with anti-aliased edges without
relying on multi-sample anti-aliasing.

#### Scenario: Smooth rounded corners

- **WHEN** a rounded rectangle or field is drawn
- **THEN** its corners and border SHALL be anti-aliased rather than showing hard stair-steps

#### Scenario: Graceful fallback

- **WHEN** no texture registry is available for the anti-aliased path
- **THEN** the style layer SHALL fall back to an equivalent non-anti-aliased path without failing
