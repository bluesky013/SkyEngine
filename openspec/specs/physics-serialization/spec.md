# physics-serialization Specification

## Purpose
TBD - created by archiving change physics-core-redesign. Update Purpose after archive.
## Requirements
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

Reflection registration and asset/component serialization for physics SHALL be implemented in the physics plugin, while engine descriptor structs and their field names SHALL remain backend-neutral. Metadata SHALL be registered only by the unique module definition owner and SHALL retain a code lease for persistent callbacks. Runtime Init/Shutdown SHALL NOT repeat reflection registration.

#### Scenario: Engine stays implementation-free
- **WHEN** the engine physics module is inspected
- **THEN** it SHALL define descriptors and field names but SHALL NOT perform reflection registration or asset serialization

#### Scenario: Plugin registers serialization
- **WHEN** the physics module definition is created
- **THEN** its metadata owner SHALL register the physics components, descriptors and config types once

#### Scenario: Runtime is restarted
- **WHEN** the same module definition begins a new runtime session
- **THEN** its existing metadata SHALL be reused without duplicate reflection or an idempotent registration guard

### Requirement: Backend migration preserves serialized identity

Replacing the provider SHALL preserve the `Physics` subsystem name, `PhysicsBody` component name, existing sky::phy type identities, enum values and serialized field names. Additive fields SHALL have explicit defaults. Saved data SHALL NOT contain SDK types or require a legacy Bullet module to deserialize.

#### Scenario: Existing world is loaded
- **WHEN** a scene saved with the existing PhysicsBody component and PhysicsSubSystemConfig is opened after migration
- **THEN** those types SHALL resolve to the Jolt plugin's registrations without changing their stored identities

### Requirement: Shape collections are fully serialized

Shape reflection SHALL preserve heightfield samples, convex hull points and recursive compound children in addition to all existing scalar fields. Runtime mesh pointers SHALL NOT be persisted as native addresses; asset identity SHALL remain the source for asset-backed mesh resolution.

#### Scenario: Compound shape round-trip
- **WHEN** a compound containing a heightfield and convex child is serialized and read back
- **THEN** its child hierarchy, pivots, height samples and hull points SHALL match the original descriptors

#### Scenario: Older data lacks newly registered collections
- **WHEN** older data omits a collection field
- **THEN** it SHALL be read using that field's documented default and any resulting invalid shape SHALL be reported explicitly rather than invented

