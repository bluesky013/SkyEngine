## ADDED Requirements

### Requirement: Backend capability descriptor

The physics backend contract SHALL expose a capability descriptor reporting at least the active simulation math mode (`Exact` or `Fast`) and whether that mode is deterministic, whether it supports job-based stepping, supports continuous collision detection, supports constraints, supports characters, and which shape kinds it implements.

#### Scenario: Capabilities are queryable before simulation

- **WHEN** a backend is registered
- **THEN** the engine SHALL be able to query its capabilities without creating a world

#### Scenario: Engine adapts to capabilities

- **WHEN** a feature is requested that the backend does not advertise
- **THEN** the engine SHALL report the feature as unavailable instead of invoking unsupported behavior

#### Scenario: Backend reports shape support

- **WHEN** a body uses a shape kind the backend does not implement
- **THEN** the capability query SHALL let a consumer detect this before authoring the body

### Requirement: World creation contract

The backend SHALL create and destroy physics worlds through the engine contract, and each created world SHALL own the objects created through it.

#### Scenario: Create a world

- **WHEN** the engine requests a world from the backend
- **THEN** the backend SHALL return a world implementing the engine world contract, or report failure when it cannot

#### Scenario: World owns its objects

- **WHEN** a world is destroyed
- **THEN** all objects created through it SHALL be destroyed with it and their handles SHALL become invalid

### Requirement: Single active backend registration

The engine registry SHALL maintain exactly one active backend at a time; registering a new backend SHALL shut down and release the previously active backend.

#### Scenario: Replace the active backend

- **WHEN** a new backend is registered while one is active
- **THEN** the previous backend's shutdown hook SHALL run and its global state SHALL be released
