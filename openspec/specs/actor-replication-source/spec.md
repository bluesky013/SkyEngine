# actor-replication-source Specification

## Purpose
TBD - created by archiving change actor-replication-source. Update Purpose after archive.
## Requirements
### Requirement: Deterministic batch iteration in the source

The framework replication source SHALL iterate replicated components grouped by type, in an order that is deterministic and independent of actor/component container layout. The source SHALL build this view itself (`World` exposes only the actor list; no per-type index is added to the framework).

#### Scenario: Grouped by type

- **WHEN** the replication source builds a snapshot over the framework world
- **THEN** it SHALL iterate replicated components grouped by type, not per-actor

#### Scenario: Layout-independent order

- **WHEN** actors or components are added and removed so container order changes
- **THEN** the replication iteration order SHALL remain stable (ordered by stable actor id)

### Requirement: Framework implements the replication seam

The framework SHALL provide an `IReplicationSource` implementation over `Actor`/`ComponentBase` that the same algorithm can use as the ECS adapter, without the algorithm depending on `Framework`.

#### Scenario: Same algorithm, framework source

- **WHEN** a replication source is provided by the framework component model
- **THEN** the replication algorithm SHALL operate over it without source-specific branching

### Requirement: Field selection from metadata

The framework replication source SHALL select replicated fields from the component reflection metadata and encode/apply them through the existing reflection and binary serialization.

#### Scenario: Unmarked fields are not replicated

- **WHEN** a component member is not marked replicated
- **THEN** its value SHALL NOT be encoded

#### Scenario: Marked fields use reflection

- **WHEN** a replicated field is encoded or applied
- **THEN** it SHALL use the existing reflection member and binary archive for its type

### Requirement: Defined apply point and host-owned lifecycle

The network host SHALL resolve spawn/despawn outside component/actor iteration, on the same thread, after the world update. The host owns actor creation and destruction and SHALL transmit the actor's full identity so the source can locate it; the source SHALL only add/remove replica components on actors that already exist.

#### Scenario: Spawn applied after tick

- **WHEN** spawn messages are received
- **THEN** the host SHALL create the actor (with its full identity) after `World::Tick` returns, not during iteration

#### Scenario: Source attaches replica components

- **WHEN** a replica is created for an entity whose actor already exists
- **THEN** the source SHALL add the replicated component to that actor

#### Scenario: No structural change during tick

- **WHEN** the world is ticking
- **THEN** the host SHALL NOT create or destroy actors

### Requirement: Side-effect-free apply

Applying replicated values SHALL NOT trigger gameplay side effects (for example component reconstruction on a field setter).

#### Scenario: Apply does not reconstruct

- **WHEN** a replicated field whose setter would normally rebuild state is applied on a client replica
- **THEN** the apply SHALL suppress that side effect

### Requirement: Client replicas are read-only

Clients SHALL apply received state and SHALL NOT propagate applied state as an authoritative change; authority SHALL be owned by the network layer.

#### Scenario: Applied state does not echo upstream

- **WHEN** a client applies replicated state
- **THEN** the applied value SHALL NOT be sent upstream as an authoritative change

