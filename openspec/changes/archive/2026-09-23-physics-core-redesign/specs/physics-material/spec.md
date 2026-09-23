## MODIFIED Requirements

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

## ADDED Requirements

### Requirement: Per-body material override

A body SHOULD be able to override or disable the world default material, and the backend SHALL apply the effective material to the body's contacts.

#### Scenario: Body overrides default material

- **WHEN** a body carries an explicit material
- **THEN** the backend SHALL apply that material's values instead of the world default

#### Scenario: Body without a material uses the default

- **WHEN** a body carries no material
- **THEN** the world default material SHALL apply
