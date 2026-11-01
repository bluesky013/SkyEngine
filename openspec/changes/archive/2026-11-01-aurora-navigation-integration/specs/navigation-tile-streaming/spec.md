## ADDED Requirements

### Requirement: Owned layered manifest

Streaming SHALL own payloads grouped by coordinate and preserve every layer. The per-frame budget SHALL count coordinates. Replacing a manifest or detaching a world SHALL NOT leave work referring to released payloads.

#### Scenario: Caller releases source

- **WHEN** the caller releases setup data
- **THEN** later paging SHALL remain valid

#### Scenario: Layered coordinate

- **WHEN** one coordinate contains multiple layers
- **THEN** paging SHALL load all those layers

#### Scenario: Replace manifest

- **WHEN** setup replaces a manifest
- **THEN** stale data SHALL NOT be installed into the new mesh

### Requirement: Query invalidation before streaming mutation

Streaming SHALL cancel in-flight path results before adding or removing tile geometry, while worker tasks retain their inputs until actual completion.

#### Scenario: Tile mutation during a path query

- **WHEN** paging changes mesh tiles while a path query is in flight
- **THEN** the query SHALL be marked canceled and its completed output SHALL be invalid

### Requirement: Failed coordinates retain bounded retry state

Untried desired coordinates SHALL take precedence over retries. Failures SHALL back off with a bounded retry delay so a permanently invalid nearest coordinate cannot consume every frame's installation budget. An unsuccessful partial-coordinate rollback or unload SHALL remain tracked and be retried before adding that coordinate's layers again, even after focus moves away. Coordinate removal SHALL be idempotent on an initialized mesh so an already-absent first-layer failure cannot permanently block retry. Pending load count SHALL describe desired coordinates awaiting installation, including retry delay, rather than disk IO.

#### Scenario: Bad nearest coordinate

- **WHEN** the coordinate budget is one and the nearest coordinate repeatedly fails
- **THEN** farther healthy desired coordinates SHALL still be installed and repeated failures SHALL not be attempted every tick

#### Scenario: Rollback itself fails

- **WHEN** a layer installation fails and removal also fails
- **THEN** the runtime SHALL retain cleanup state and confirm removal before attempting a new installation for the coordinate

#### Scenario: First layer fails without installing geometry

- **WHEN** first-layer installation fails and leaves the coordinate absent
- **THEN** cleanup SHALL confirm absence successfully and a later retry SHALL be allowed after the failure cause is removed
