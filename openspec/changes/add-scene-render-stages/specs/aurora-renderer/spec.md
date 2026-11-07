## ADDED Requirements

### Requirement: Scene extension registration and retirement

The renderer SHALL accept coordinator-owned scene extension registration and retirement commands, validate identity and duplicates, and retain extensions only until their CPU/GPU borrowers retire.

#### Scenario: Register an extension

- **WHEN** a host registers a scene extension with the active scene feature
- **THEN** the renderer SHALL validate the extension name and identity and make it available to the scene build

#### Scenario: Duplicate extension

- **WHEN** a host registers a second extension with the same identity for the same scene
- **THEN** the renderer SHALL reject the registration without replacing the existing extension

#### Scenario: Retire an extension

- **WHEN** a host retires a scene extension
- **THEN** the renderer SHALL stop invoking it and SHALL call its retirement hook only after all frames that used it complete

#### Scenario: Unknown scene feature

- **WHEN** a host registers or retires an extension for a scene feature that is not active
- **THEN** the renderer SHALL return an explicit error without modifying another scene

#### Scenario: No retained borrowed context

- **WHEN** an extension callback returns
- **THEN** the renderer SHALL NOT retain its borrowed build context, graph handles or view reference
