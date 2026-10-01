# network-replication-source Specification

## Purpose
TBD - created by archiving change add-network-replication. Update Purpose after archive.
## Requirements
### Requirement: Replication reads through a source seam

Replication SHALL read simulation state through an engine-side replication-source interface defined in the algorithm target (`NetworkReplication`), rather than depending on a concrete world model such as `Actor`/`ComponentBase`. The algorithm target SHALL NOT depend on `Framework` or on a concrete simulation model.

#### Scenario: Source supplied by the host

- **WHEN** a world is set up for replication
- **THEN** the replication layer SHALL obtain state through the replication-source interface and SHALL NOT reference a concrete world model type

#### Scenario: Algorithm target is Framework-free

- **WHEN** the algorithm target is built
- **THEN** it SHALL NOT link `Framework`, so a pure-ECS adapter can be used without it

### Requirement: Source is adaptable by both the component model and the ECS

The source seam SHALL be implementable by both the `Actor`/`ComponentBase` model (adapter target `NetworkWorld`, links `Framework`) and the data-oriented ECS (adapter target `NetworkEcs`, links `Core`), without changing the replication algorithm.

#### Scenario: Two adapters, one algorithm

- **WHEN** a replication source is provided by the component model and, separately, by the ECS
- **THEN** the same replication algorithm SHALL operate over both without source-specific branching

### Requirement: Iteration is batched and dense

The source SHALL expose replicated entities/components in batches by type, iterated densely, so encoding does not perform per-entity virtual dispatch.

#### Scenario: Batch per type

- **WHEN** the replication layer builds a snapshot
- **THEN** it SHALL iterate replicated components grouped by type in dense batches

### Requirement: Network type identity is stable across modules

Replicated types SHALL be identified by a cross-module, call-order-independent identifier (such as the ECS compile-time tag-hash `TypeId` or a component `Uuid`). Runtime reflection SHALL NOT be used as the network identity.

#### Scenario: Identity consistent across modules

- **WHEN** the same replicated type is referenced from two modules or plugins
- **THEN** its network identifier SHALL be identical regardless of call order or module

### Requirement: Replicated state is stored in field columns

The source SHALL expose replicated state as per-field columns (structure of arrays) so delta, quantization, and bandwidth budgeting operate on fields in batches.

#### Scenario: Field-wise delta

- **WHEN** a snapshot delta is computed for a type
- **THEN** it SHALL be computed per field column rather than per entity object

### Requirement: Iteration order is deterministic

The source SHALL iterate in a deterministic order independent of underlying container layout, so `SparseSet` swap-remove reordering cannot change snapshot content or delta baselines.

#### Scenario: Order independent of container layout

- **WHEN** entities are removed and re-added so the underlying dense array order changes
- **THEN** the replication iteration order SHALL remain stable

