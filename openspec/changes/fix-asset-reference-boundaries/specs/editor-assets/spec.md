## ADDED Requirements

### Requirement: Engine creation defaults use explicit source ownership
Editor Engine defaults SHALL resolve from the Engine mount at the owning creation or preview boundary. Explicit project material configuration SHALL remain authoritative. Asset reference fields SHALL retain readable labels and hover details.

#### Scenario: Project contains same-name default source
- **WHEN** Project contains a file with the same relative path as an Engine creation mesh
- **THEN** the Engine creator retains the Engine mesh UUID
- **AND** the project-configured material is applied independently

### Requirement: Derived editor data uses writable host storage
Editor derived-data caching SHALL use writable host storage outside deployed application resources. Unavailable persistence SHALL be diagnosed while keeping successful in-memory derivation usable.

#### Scenario: Read-only application installation
- **WHEN** the deployed editor resource directory is read-only
- **THEN** icon derivation uses writable host cache storage without modifying installed resources
