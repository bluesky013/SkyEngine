## ADDED Requirements

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
