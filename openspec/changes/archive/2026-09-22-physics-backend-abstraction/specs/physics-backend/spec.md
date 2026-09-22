## ADDED Requirements

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
