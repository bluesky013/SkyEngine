## ADDED Requirements

### Requirement: Stable descriptor field names

Physics descriptors SHALL have stable, documented serialized field names and a version marker, so that saved physics data can be read back across code changes.

#### Scenario: Field names are stable

- **WHEN** a physics descriptor is serialized
- **THEN** the emitted field names SHALL be documented, correctly spelled, and not silently renamed between versions

#### Scenario: Shape data round-trips

- **WHEN** a shape descriptor is serialized and deserialized
- **THEN** all of its data required to rebuild the shape SHALL be preserved, with no field omitted

### Requirement: Body descriptor round-trip

A body descriptor SHALL serialize and deserialize without loss, preserving kind, shape, mass, damping, material reference, filter, CCD, gravity, and initial transform.

#### Scenario: Body round-trip

- **WHEN** a body descriptor is serialized and then deserialized
- **THEN** the reconstructed descriptor SHALL equal the original

### Requirement: Physics component round-trip

Physics components SHALL serialize and deserialize so that saving and restoring a scene preserves each component's physics configuration.

#### Scenario: Component round-trip

- **WHEN** an actor with a physics component is serialized and restored
- **THEN** the component SHALL be recreated with the same configuration and SHALL re-establish its physics object through a handle

### Requirement: Material and filter round-trip

Material data and collision filter data SHALL serialize and deserialize without loss, including combine modes, damping, and group/mask values.

#### Scenario: Material and filter round-trip

- **WHEN** material and filter data are serialized and deserialized
- **THEN** the reconstructed values SHALL equal the originals

### Requirement: Serialization implementation lives in the plugin

Reflection registration and asset/component serialization for physics SHALL be implemented in the physics plugin, while the descriptor structs and their field names SHALL live in the engine module.

#### Scenario: Engine stays implementation-free

- **WHEN** the engine physics module is inspected
- **THEN** it SHALL define descriptors and field names but SHALL NOT perform reflection registration or asset serialization

#### Scenario: Plugin registers serialization

- **WHEN** the physics plugin module starts
- **THEN** it SHALL register the physics components and descriptor types for serialization
