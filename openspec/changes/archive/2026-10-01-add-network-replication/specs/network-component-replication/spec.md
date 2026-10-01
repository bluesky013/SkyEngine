## ADDED Requirements

### Requirement: Components declare which fields replicate

A component SHALL be able to mark a subset of its serialized fields as replicated, and non-marked fields SHALL NOT be sent to clients.

#### Scenario: Unmarked fields stay server-only

- **WHEN** a component has a field not marked as replicated
- **THEN** that field's value SHALL NOT appear in any replication message

### Requirement: Replication reuses the existing reflection and binary serialization

Replicated-field metadata SHALL be built on the existing reflection system and component binary serialization rather than a separate serialization path.

#### Scenario: Replicated serialization uses existing types

- **WHEN** a replicated field of a registered type is encoded
- **THEN** it SHALL use the existing reflection and binary archive types for that type

### Requirement: Clients create and destroy replicas from replication messages

The client SHALL create a replica for an entity on spawn and remove it on despawn, driven by replication messages rather than by load-time world data.

#### Scenario: Replica created on spawn

- **WHEN** the client receives a spawn for a replicated entity
- **THEN** the client SHALL instantiate the entity and its replicated components

#### Scenario: Replica removed on despawn

- **WHEN** the client receives a despawn for a replicated entity
- **THEN** the client SHALL remove that entity and its replicated components

### Requirement: Delta state uses a replicated-field bitmask

Each replicated component SHALL encode its delta as a bitmask of changed replicated fields followed by the changed values, so unchanged fields are omitted.

#### Scenario: Only changed fields are encoded

- **WHEN** one replicated field of a component changes
- **THEN** the encoded delta SHALL contain that field's bit and value and SHALL omit the unchanged fields
