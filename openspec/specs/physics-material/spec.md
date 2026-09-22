# physics-material Specification

## Purpose
TBD - created by archiving change physics-backend-abstraction. Update Purpose after archive.
## Requirements
### Requirement: Physics material data

The physics module SHALL define backend-neutral material data containing static friction, dynamic friction, and restitution, and the backend factory SHALL provide a creation method for materials.

#### Scenario: Create a material

- **WHEN** a material is created from friction and restitution values
- **THEN** the physics module SHALL produce a backend-neutral material usable by collision objects and rigid bodies

### Requirement: Objects consume materials

Collision objects and rigid bodies SHALL apply the material assigned to them, and the engine SHALL NOT hardcode friction or restitution in the backend.

#### Scenario: Material values reach the backend

- **WHEN** a collision object or rigid body is assigned a material
- **THEN** the backend SHALL apply that material's friction and restitution to the underlying object

#### Scenario: Default material

- **WHEN** an object is created without an explicit material
- **THEN** a documented default material SHALL be applied rather than a backend-specific constant

### Requirement: Bullet backend material implementation

The Bullet backend SHALL implement material creation and apply material friction and restitution to Bullet objects, replacing the previously hardcoded values.

#### Scenario: Bullet applies material

- **WHEN** a material is applied to a Bullet-backed object
- **THEN** the Bullet object's friction and restitution SHALL equal the material values

