## MODIFIED Requirements

### Requirement: Generic product transactions publish complete generations
Framework SHALL expose explicit-result product-group staging and publication. A transaction SHALL validate encoding, required writes, flush and readback before one atomic manifest replacement makes its complete member set visible. Failed or stale transactions SHALL leave the previous committed generation readable. Unsupported filesystem guarantees SHALL return an explicit failure.

#### Scenario: Failure before publication
- **WHEN** a write, flush, validation or commit fails after some members were staged
- **THEN** no staged member becomes the published generation and the transaction reports failure

#### Scenario: Stale writer or interrupted process
- **WHEN** a superseded cook finishes or a process stops before manifest publication
- **THEN** the latest successfully committed generation remains authoritative

### Requirement: Product snapshots retain their selected generation
AssetManager SHALL bind product headers and bulk reads to immutable snapshots. Reload SHALL publish new objects without mutating retained snapshots. Products-only loads and inspection SHALL resolve product metadata without requiring a source catalog or starting a cook.

#### Scenario: Reload with retained readers
- **WHEN** a new generation is published while a consumer retains an older snapshot
- **THEN** the retained header and bulk reads continue to address the older generation

#### Scenario: Product inspection without sources
- **WHEN** a product is inspected with no source catalog
- **THEN** its type, dependencies and pinned payload are returned from product metadata without mutating the asset cache

### Requirement: Generated child requests have individual terminal results
Framework SHALL route a generated child cook request to its source owner and coalesce concurrent requests by owner and target. Every child waiter SHALL complete according to the presence and validity of its own requested member. Failed cooks and removed children SHALL produce explicit failure rather than indefinite Loading or inherited owner success.

#### Scenario: Concurrent children of one source
- **WHEN** two generated children require the same owner cook and target
- **THEN** one cook serves both requests and each child receives its own terminal result

#### Scenario: Child omitted by a successful cook
- **WHEN** the owner succeeds but the requested child no longer exists
- **THEN** that child completes with failure
