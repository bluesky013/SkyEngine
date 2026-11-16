## MODIFIED Requirements

### Requirement: Path resolution has an editor-only source fallback

For AllowCook path loads with an editor source catalog, the loader SHALL resolve the selected source identity first and consult product indexes only on a source miss. ProductsOnly and runtime loads SHALL consult product indexes only. Missing paths and product paths resolving to different UUIDs across indexes SHALL report missing. Adding an uncooked source SHALL NOT itself rewrite a published product index.

#### Scenario: Built asset resolves via the product index
- **WHEN** a ProductsOnly path load matches one unambiguous product identity
- **THEN** the loader SHALL resolve that identity without consulting the source catalog

#### Scenario: Uncooked source resolves via the editor fallback
- **WHEN** an AllowCook path load matches an uncooked Project source overlaying an Engine product
- **THEN** the loader SHALL select the Project UUID and proceed to on-demand cook

#### Scenario: Runtime path miss reports missing
- **WHEN** a runtime path load misses the product index
- **THEN** the load SHALL fail without consulting source catalogs

#### Scenario: Product path is ambiguous
- **WHEN** product lookup is required and the path matches different UUIDs across product indexes
- **THEN** the load SHALL report missing rather than selecting an arbitrary product

### Requirement: Checked product saves report the actual publication result

SaveAsset and SaveProducts SHALL return explicit ProductStatus carrying publication and recovery state. A batch SHALL encode, validate and stage all products with required write, flush and readback checks before replacing destinations. Each destination replacement SHALL be atomic. A failure SHALL propagate through the builder and package flow; no failed batch SHALL be reported as a successful cook. Partial replacement SHALL invalidate the affected batch without claiming restoration of the previous complete batch. An index failure after all validated products have been replaced SHALL retain those products, invalidate loaded-cache entries, and report CommittedNeedsRepair; UUID-addressed reads may validate and load them, but package/cook completion remains failed until repair. Compensation failures SHALL report RollbackFailed with residual paths and stages.

#### Scenario: Encoding or staging fails
- **WHEN** an input fails encoding, a staged write or flush fails, or readback does not match
- **THEN** the batch reports failure without replacing any destination
- **AND** any retained temporary file is reported with its recovery stage and path

#### Scenario: Failure after one destination was replaced
- **WHEN** a later atomic replacement fails
- **THEN** the batch is invalidated and reports that recook is required
- **AND** failed invalidation cleanup identifies residual products without claiming rollback

#### Scenario: Index fails after validated products commit
- **WHEN** all products are replaced but index persistence fails
- **THEN** the save reports CommittedNeedsRepair and retains the validated products
- **AND** a later successful recook repairs the index before reporting cook success

## ADDED Requirements

### Requirement: Index readers retain immutable published state
Manifest and product-index readers SHALL retain immutable snapshots for the lifetime of borrowed entry views. Authoritative reload SHALL NOT publish candidates. Failed persistence SHALL NOT publish candidate state. Cache initialization, refresh and writes SHALL serialize publication or reject stale publication generations.

#### Scenario: A reader outlives cache replacement
- **WHEN** a writer saves, invalidates or clears the cache while a reader retains a snapshot
- **THEN** the reader SHALL retain unchanged valid entries and subsequent readers SHALL observe the newly published or reloaded state

### Requirement: Damaged source directories remain read only
Partial parsing SHALL retain only independently valid entries and report physical line diagnostics. Duplicate key or identity participants SHALL be isolated. Damaged directories SHALL NOT be seeded, migrated or rewritten by scanning. Legitimate manifest identities SHALL supersede stale development-cache identities.

#### Scenario: Valid and malformed lines share a directory
- **WHEN** a directory manifest contains valid entries and malformed or conflicting lines
- **THEN** scanning SHALL report the blocked directory, retain independently valid identities and leave persisted bytes unchanged

### Requirement: Mutation completion distinguishes durable commit from repair
Source operations SHALL distinguish no commit, durable commit and durable commit needing derived-state repair. Failed compensation SHALL identify residual paths and stages. Generated catalog repair SHALL replay authoritative source metadata; product repair SHALL validate or rebuild products before indexing them.

#### Scenario: Derived publication fails after source commit
- **WHEN** source metadata commits but generated catalog publication fails
- **THEN** the operation SHALL report committed state requiring repair and SHALL NOT claim a rollback or silently delete the committed source

#### Scenario: Delete commits but staged-byte cleanup fails
- **WHEN** the source identity removal commits and deleting the staged source file fails
- **THEN** the source remains unpublished and the result reports CommittedNeedsRepair with the staged path
- **AND** it does not restore the deleted identity merely to hide a cleanup failure

#### Scenario: Move compensation fails
- **WHEN** a move cannot complete and restoring its files or manifests fails
- **THEN** the result reports RollbackFailed with affected paths and stages
- **AND** the in-memory catalog does not continue advertising the old source location

### Requirement: Cook submission captures owned source fields

Cook submission SHALL capture source fields and their owning filesystem under memory protection before performing input I/O or queue submission. Captured field values SHALL survive subsequent source moves, metadata changes and removal. Registration SHALL release its asset-memory lock before opening the cook input and submitting the request. This field snapshot does not replace the separate input-version and publication validation protocol.

#### Scenario: Source changes after capture
- **WHEN** a registered source is moved, renamed or removed after its fields are captured
- **THEN** the retained snapshot SHALL preserve its original path, name, dependencies and owning filesystem

#### Scenario: Registration schedules a cook
- **WHEN** registration opens its cook input
- **THEN** a concurrent memory-only source reader SHALL not wait for that input I/O to finish

### Requirement: Versioned cook publication
Cook requests SHALL retain owned source bytes, identity, path, mount and effective settings. Coalescing SHALL include these input versions and the local source/configuration incarnation. Source authoring replacement and generated-identity commits SHALL use the source directory serialization protocol. Obsolete generated metadata and product publication SHALL be rejected before replacement. Declared builder configuration files SHALL be checked for changes; a preset reload invalidates previous in-process requests. External workers SHALL carry the same input fingerprint and report actual builder failure. All completion subscribers SHALL complete on success, failure, cancellation or stale input.

#### Scenario: Source changes during cook
- **WHEN** source bytes, source incarnation or cook settings change after submission
- **THEN** the old request cannot replace products or commit generated identities for the new input

#### Scenario: A worker has an older source overlay snapshot
- **WHEN** a newer worker has published a Project mapping that the older catalog does not own or has not observed
- **THEN** the older publication retains that mapping while its Engine product remains addressable by UUID

### Requirement: Separate asset memory and file-lock phases
Asset memory readers SHALL NOT wait for source filesystem I/O under assetMutex. Source writers SHALL serialize before taking directory locks, release filesystem locks before publishing mutable catalog memory, and capture owned data before submitting cook work. Busy directory locks SHALL use bounded retries and observe reset cancellation.

#### Scenario: Manifest persistence is delayed
- **WHEN** a writer is waiting for or writing a manifest
- **THEN** an independent reader can obtain the last published source snapshot
