# physics-material Specification

## Purpose
TBD - created by archiving change physics-backend-abstraction. Update Purpose after archive.
## Requirements
### Requirement: Physics material data

The physics module SHALL define backend-neutral material data containing static friction, dynamic friction, restitution, friction and restitution combine modes, and linear and angular damping, and the backend factory SHALL provide a creation method for materials.

#### Scenario: Create a material

- **WHEN** a material is created from friction, restitution, combine modes, and damping values
- **THEN** the physics module SHALL produce a backend-neutral material usable by bodies

#### Scenario: Combine modes are carried

- **WHEN** a material specifies a friction or restitution combine mode
- **THEN** the material data SHALL preserve that mode and pass it to the backend for the contact computation

#### Scenario: Damping is carried

- **WHEN** a material specifies linear or angular damping and is assigned to a body
- **THEN** the backend SHALL apply the damping to that body

### Requirement: Objects consume materials

Collision objects and rigid bodies SHALL apply the material assigned to them, and the engine SHALL NOT hardcode friction or restitution in the backend.

#### Scenario: Material values reach the backend

- **WHEN** a collision object or rigid body is assigned a material
- **THEN** the backend SHALL apply that material's friction and restitution to the underlying object

#### Scenario: Default material

- **WHEN** an object is created without an explicit material
- **THEN** a documented default material SHALL be applied rather than a backend-specific constant

### Requirement: Per-body material override

A body SHOULD be able to override or disable the world default material, and the backend SHALL apply the effective material to the body's contacts.

#### Scenario: Body overrides default material

- **WHEN** a body carries an explicit material
- **THEN** the backend SHALL apply that material's values instead of the world default

#### Scenario: Body without a material uses the default

- **WHEN** a body carries no material
- **THEN** the world default material SHALL apply

### Requirement: Jolt material mapping has explicit supported semantics

The Jolt backend SHALL apply effective friction, restitution and damping from engine material data. Its delivered single-friction contact model SHALL require equal static and dynamic friction values and SHALL reject unequal values as Unsupported rather than discard one. Invalid handles, non-finite values and invalid coefficients SHALL fail before mutation.

#### Scenario: Supported material is applied
- **WHEN** a material with equal valid static/dynamic friction is assigned
- **THEN** its friction, restitution and effective damping SHALL be applied to the body and subsequent contacts

#### Scenario: Separate friction coefficients are requested
- **WHEN** staticFriction differs from dynamicFriction
- **THEN** material creation or assignment SHALL return Unsupported and preserve existing body material state

#### Scenario: A stale material handle is reused
- **WHEN** a deleted material's slot has been reused and the old handle is supplied
- **THEN** the request SHALL fail rather than select the new material

### Requirement: Contact combine rules are backend-independent

The backend SHALL combine the two effective material values using their selected Average, Min, Multiply or Max mode. When the materials select different modes, priority SHALL be `Max > Multiply > Min > Average`. The computation SHALL be symmetric in body ordering and SHALL be applied to both friction and restitution.

#### Scenario: Different combine modes meet
- **WHEN** one material selects Average and the other selects Max
- **THEN** the corresponding contact coefficient SHALL be the maximum of the two values regardless of body order

### Requirement: Damping limitations and precedence are explicit

The delivered backend SHALL accept isotropic per-body damping and reject unequal XYZ components as Unsupported. A nonzero isotropic body damping value SHALL override the corresponding material damping; an all-zero vector SHALL select that material damping. Later material reassignment SHALL preserve explicit body damping overrides.

#### Scenario: Body damping overrides a material
- **WHEN** a body provides equal nonzero linear damping components
- **THEN** those components SHALL define its scalar linear damping even after another material is assigned

#### Scenario: Directional damping is requested
- **WHEN** a body descriptor supplies unequal damping components
- **THEN** creation SHALL fail explicitly without silently reducing them to a scalar

