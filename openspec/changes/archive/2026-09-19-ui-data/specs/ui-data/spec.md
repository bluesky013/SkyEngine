## ADDED Requirements

### Requirement: Built-in binding converters
The binding system SHALL provide built-in converters `remap`, `format`, and `boolToVisible` that transform a
source value into a target value, with converter arguments supplied by the document.

#### Scenario: remap scales a value
- **WHEN** a binding uses `remap` with an input range and an output range
- **THEN** the target value SHALL be the source value linearly mapped from the input range into the output range

#### Scenario: boolToVisible passes a bool
- **WHEN** a binding uses `boolToVisible`
- **THEN** the target SHALL receive the source boolean value unchanged

### Requirement: Binding evaluation timing
Bindings SHALL re-evaluate when the data context changes, and SHALL NOT re-apply when the context is unchanged
since the last evaluation.

#### Scenario: Apply on change
- **WHEN** a value on the data context changes and bindings are applied
- **THEN** bound targets SHALL reflect the new value

#### Scenario: Skip when unchanged
- **WHEN** bindings are applied again without any data-context change
- **THEN** the system SHALL report that no update was needed

### Requirement: Load diagnostics
The loader SHALL return diagnostics describing content problems (unknown element type, unresolved asset,
unknown property path, duplicate name) with severity, node path, code, and message, and SHALL NOT throw.

#### Scenario: Diagnostics collected
- **WHEN** a document references an unknown element type and an unresolved texture
- **THEN** the loader SHALL return an error diagnostic for the type and a warning diagnostic for the asset
