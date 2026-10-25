## MODIFIED Requirements

### Requirement: Generated catalog records preserve child identity and ownership
The existing source manifest and catalog SHALL record generated children by stable source-local key, UUID and type, together with their source owner and optional default-binding metadata. Recook and source moves SHALL preserve matching child identities; copying a source SHALL allocate independent child identities. Removed children SHALL no longer resolve as current catalog entries. Ambiguous or conflicting identity assignments SHALL fail explicitly.

#### Scenario: Move and copy a generated source
- **WHEN** a source with generated children is moved and subsequently copied
- **THEN** the move preserves its child UUIDs and the copy owns distinct child UUIDs

#### Scenario: Child dependency routes through its owner
- **WHEN** a builder registers a dependency on a generated child
- **THEN** the dependency retains the child UUID while recook routing resolves the actual owner source
