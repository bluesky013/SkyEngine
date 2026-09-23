## ADDED Requirements

### Requirement: Backend-neutral constraint descriptors

The physics module SHALL describe constraints as plain data, covering at least fixed, hinge, slider, distance, and generic 6-DOF joints, with anchors, axes, limits, and a reference to the constrained bodies by `PhysicsObjectId`.

#### Scenario: Describe a joint without a backend

- **WHEN** a constraint descriptor is created
- **THEN** it SHALL reference engine data and body handles only and SHALL NOT include backend types

#### Scenario: Single-body and two-body constraints

- **WHEN** a constraint references one body and a world anchor, or two bodies
- **THEN** the descriptor SHALL express both cases

### Requirement: Constraint lifetime is world-owned

Constraints SHALL be created and removed through the world, addressed by `PhysicsObjectId`, and their lifetime SHALL follow the world.

#### Scenario: Create and remove a constraint

- **WHEN** a constraint is created and later removed
- **THEN** the world SHALL return a handle on creation and SHALL invalidate it on removal

#### Scenario: Removing a body removes its constraints

- **WHEN** a body referenced by a constraint is removed
- **THEN** the world SHALL remove the dependent constraint rather than leave a dangling reference

### Requirement: Constraint support is capability-gated

The world SHALL report which constraint kinds the active backend supports, and requesting an unsupported kind SHALL fail safely.

#### Scenario: Unsupported constraint is reported

- **WHEN** a constraint kind is requested that the backend does not implement
- **THEN** the world SHALL report it as unsupported and SHALL NOT create a partially working constraint

### Requirement: Limits and drives are describeable

Constraint descriptors SHALL express angular and linear limits and, where applicable, motor/drive targets, so gameplay can configure joints without backend-specific code.

#### Scenario: Configure limits

- **WHEN** a hinge constraint is described with angular limits
- **THEN** the backend SHALL enforce those limits in simulation
