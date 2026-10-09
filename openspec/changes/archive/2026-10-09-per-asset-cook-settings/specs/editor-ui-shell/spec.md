## ADDED Requirements

### Requirement: Editable per-asset cook settings in the asset browser

The asset browser details pane SHALL edit the selected asset's per-target cook settings through the generic
reflected form view (`ReflectedFormView`) bound to the catalog's effective reflected settings object with the
global-preset baseline, so that enums/bools/ints render as their standard typed controls and "reset"
restores the preset. Edits SHALL be committed through the asset catalog and persisted to the manifest; for
assets on a read-only mount the settings SHALL be shown non-editable.

#### Scenario: Bind the reflected form

- **WHEN** an asset with a builder-declared settings type is selected
- **THEN** the details pane binds a `ReflectedFormView` to the effective settings with the preset baseline

#### Scenario: Edit persists

- **WHEN** the user changes a value through the form
- **THEN** the catalog persists the resulting sparse override and the pane reflects the new effective value

#### Scenario: Reset restores the preset

- **WHEN** the user resets an overridden value through the form
- **THEN** the override key is removed and the shown value returns to the preset baseline

#### Scenario: Read-only mount

- **WHEN** the selected asset belongs to a read-only mount
- **THEN** the settings are displayed without editable controls
