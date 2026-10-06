## ADDED Requirements

### Requirement: Per-member replication flag

The reflection metadata SHALL provide a `REPLICATED` flag on reflected component members, registrable inline with the member.

#### Scenario: Replication flag is queryable

- **WHEN** a member is registered with the replication flag
- **THEN** a query SHALL report that member as replicated

#### Scenario: Existing keys unchanged

- **WHEN** a member is registered with the existing asset-type or visibility keys
- **THEN** its behavior SHALL be unchanged

### Requirement: Single metadata source

The `REPLICATED` flag SHALL live on the component accessor member node, and the replication adapter SHALL read it from there rather than a separate store.

#### Scenario: Data node stays serialization-only

- **WHEN** a component's data struct node is inspected
- **THEN** it SHALL NOT be the source of replication flags

### Requirement: Stable identity is independent of metadata

Type/network identity SHALL derive from a stable type identifier, not from per-member metadata or runtime reflection.

#### Scenario: Identity is metadata-independent

- **WHEN** a type's identity is computed
- **THEN** it SHALL NOT depend on which members are marked replicated
