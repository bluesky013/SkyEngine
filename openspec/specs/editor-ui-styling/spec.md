# editor-ui-styling Specification

## Purpose
TBD - created by archiving change editor-ui-antialiasing. Update Purpose after archive.
## Requirements
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

### Requirement: Default editor theme is shared across DLLs
The default UiTheme accessed through GetDefaultUiTheme and SetDefaultUiTheme SHALL use Environment-backed process-shared storage with a stable object address. Replacing default theme values SHALL update already-created shared skins/widgets that reference that default, including consumers in another DLL. Explicitly injected custom themes SHALL remain usable independently of the default.

#### Scenario: Default theme changes cross the module boundary
- **WHEN** the host changes the default theme and an already-created feature-module skin reads it
- **THEN** the skin SHALL observe the changed colors, metrics, and fonts from the same stable theme object

#### Scenario: Feature and host share default theme identity
- **WHEN** a real dynamic module and its host each access the default theme
- **THEN** both SHALL receive the same object address rather than independent function-static copies

#### Scenario: Explicit custom theme stays independent
- **WHEN** a form uses an explicitly supplied theme and the default theme changes
- **THEN** the form SHALL continue to use its supplied theme
