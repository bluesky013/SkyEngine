## ADDED Requirements

### Requirement: Skeletal product saves report checked publication outcomes
The skeletal import path SHALL build its complete required Mesh/Skin/Skeleton product list before calling the existing Framework SaveProducts API. Encoding, staging, flush, readback, per-file atomic replacement and index failures SHALL report explicit failure. A failed batch SHALL NOT emit cook success or be accepted as successful package output. No persistent product group or rollback to a previous complete group is required; storage follows simplify-asset-product-storage.

#### Scenario: Failure after replacing a mesh
- **WHEN** a later Skin/Skeleton replacement or index update fails
- **THEN** the batch reports failure and its affected output is invalidated rather than accepted as a successful partial cook

### Requirement: Immutable loaded instances during binding and reload
Skeletal products SHALL load through the existing UUID product loader using in-memory asset instance identities/revisions. Reload SHALL create new immutable loaded objects rather than mutate published data or reopen retained bulk by UUID path. Mesh/Skin/Skeleton candidates SHALL validate their complete slot/layout/reference-pose combination and fixed loaded instances before switching. Existing consumers and submitted GPU work SHALL retain active data through normal retirement. Runtime and packaging SHALL use checked product data without source access or runtime group metadata.

#### Scenario: Rebuild while a candidate is being prepared
- **WHEN** a product change invalidates a skeletal binding candidate
- **THEN** the resolver rebuilds and validates the complete candidate before replacing the active binding
- **AND** it does not mutate already-submitted frame resources

#### Scenario: Products-only package
- **WHEN** a World referencing separate Mesh/Skin/Skeleton assets and a Graph is packaged without sources
- **THEN** explicit component roots and transitive dependencies resolve through checked UUID products without group manifests

### Requirement: Generated child cook requests complete through their owner
A missing generated child with a valid catalog owner SHALL request the owner source cook, coalesced by owner and target bundle. Every child receives success only when its own requested checked product exists after successful cook. Failure or a removed child SHALL complete as Failed with a diagnostic. No child SHALL recursively schedule a nonexistent child source or remain Loading indefinitely.

#### Scenario: Concurrent skin and skeleton requests
- **WHEN** an uncooked Skin and Skeleton from one model are requested together
- **THEN** one owner cook per bundle serves both requests and both reach terminal results

#### Scenario: Requested child removed by reimport
- **WHEN** the owner cooks successfully but a requested child no longer exists
- **THEN** that child fails explicitly instead of inheriting owner success
