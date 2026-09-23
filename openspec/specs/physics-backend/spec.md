# physics-backend Specification

## Purpose
TBD - created by archiving change physics-backend-abstraction. Update Purpose after archive.
## Requirements
### Requirement: Backend lifecycle hooks

The physics backend factory SHALL expose initialization and shutdown hooks that are invoked when a backend is registered and unregistered, so a backend that requires global SDK state can bootstrap and tear it down.

#### Scenario: Initialize a backend on registration

- **WHEN** a physics backend is registered
- **THEN** its initialization hook SHALL run before any physics world or shape is created

#### Scenario: Shut down a backend on unregistration

- **WHEN** a physics backend is unregistered
- **THEN** its shutdown hook SHALL run and release backend-global state

#### Scenario: A backend with no global state

- **WHEN** a backend does not require global state (for example the Bullet backend)
- **THEN** its lifecycle hooks SHALL be satisfiable as no-ops without changing engine behavior

### Requirement: Heightfield collision shape

The physics module SHALL describe a heightfield collision shape as backend-neutral data comprising grid width, grid height, height samples with a declared sample format, height scale, height offset, and up axis, and the backend factory SHALL provide a creation method for it.

#### Scenario: Describe a heightfield

- **WHEN** a heightfield shape description is created with grid dimensions, samples, scale, offset, and up axis
- **THEN** the physics module SHALL accept it without referencing any backend type

#### Scenario: Backend creates a heightfield shape

- **WHEN** the backend factory creates a shape from a heightfield description
- **THEN** the backend SHALL produce a collision shape matching the described grid and vertical mapping

### Requirement: Capsule collision shape

The physics module SHALL describe a capsule collision shape (radius, height, pivot) as backend-neutral data and the backend factory SHALL provide a creation method for it.

#### Scenario: Create a capsule shape

- **WHEN** the backend factory creates a shape from a capsule description
- **THEN** the backend SHALL produce a capsule collision shape with the described radius and height

### Requirement: Shape creation delegates cooking to the backend

Shape descriptions SHALL carry plain data only; any backend-specific preparation (such as cooking a heightfield or triangle mesh) SHALL occur inside the backend, and the engine-side shape contract SHALL NOT expose backend types.

#### Scenario: Cooking stays in the backend

- **WHEN** a backend requires cooked geometry for a shape
- **THEN** the cooking SHALL happen inside the backend and the engine-side description SHALL remain unchanged

### Requirement: Bullet backend implements the extended shape set

The Bullet backend SHALL implement heightfield and capsule shapes so that existing box, sphere, and triangle-mesh behavior is preserved.

#### Scenario: Bullet heightfield shape

- **WHEN** the Bullet backend creates a heightfield shape from a heightfield description
- **THEN** it SHALL produce a Bullet heightfield collision shape positioned and scaled to match the description

#### Scenario: Existing shapes unchanged

- **WHEN** the Bullet backend creates a box, sphere, or triangle-mesh shape
- **THEN** their behavior SHALL match the behavior before this change

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

