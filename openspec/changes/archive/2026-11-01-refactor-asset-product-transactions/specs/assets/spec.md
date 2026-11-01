## MODIFIED Requirements

### Requirement: Generated child requests have individual terminal results

Framework SHALL route a generated child cook request to its source owner and coalesce concurrent requests by owner and target. Every child waiter SHALL complete according to the presence and validity of its own requested member. Failed cooks and removed children SHALL produce explicit failure rather than indefinite Loading or inherited owner success.

#### Scenario: Concurrent children of one source
- **WHEN** two generated children require the same owner cook and target
- **THEN** one cook serves both requests and each child receives its own terminal result

#### Scenario: Child omitted by a successful cook
- **WHEN** the owner succeeds but the requested child no longer exists
- **THEN** that child completes with failure

### Requirement: Generated catalog records preserve child identity and ownership

The existing source manifest and catalog SHALL record generated children by stable source-local key, UUID and type, together with their source owner and optional default-binding metadata. Recook and source moves SHALL preserve matching child identities; copying a source SHALL allocate independent child identities. Removed children SHALL no longer resolve as current catalog entries. Ambiguous or conflicting identity assignments SHALL fail explicitly.

#### Scenario: Move and copy a generated source
- **WHEN** a source with generated children is moved and subsequently copied
- **THEN** the move preserves its child UUIDs and the copy owns distinct child UUIDs

#### Scenario: Child dependency routes through its owner
- **WHEN** a builder registers a dependency on a generated child
- **THEN** the dependency retains the child UUID while recook routing resolves the actual owner source
