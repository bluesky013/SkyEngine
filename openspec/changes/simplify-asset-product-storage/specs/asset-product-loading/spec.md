## REMOVED Requirements

### Requirement: Generic product transactions publish complete generations
**Reason**: The first-release storage contract removes owner manifests and durable product-group generations.
**Migration**: Clear existing development cook output and rebuild flat products; no compatibility reader or automatic migration is provided.

### Requirement: Product snapshots retain their selected generation
**Reason**: Runtime no longer exposes product-group snapshots or historical disk generations.
**Migration**: Retained loaded instances keep active CPU/bulk/GPU data; later loads use fresh instance identities after publication.

## ADDED Requirements

### Requirement: Flat products have a checked versioned identity envelope
Each cooked asset SHALL use one UUID-addressed product file at `<uuid[0:2]>/<uuid>.bin`. The file SHALL begin with a 40-byte prefix containing magic `0x44525053`, version `1`, the UUID, exact body byte length and FNV-1a64 body checksum. The loader and inspection SHALL reject unsupported versions, wrong identity, truncation, trailing bytes and checksum mismatches before accepting decoded content. There SHALL be no group manifest or legacy/headerless product fallback.

#### Scenario: Corrupt or incompatible product
- **WHEN** a product has a wrong magic/version/UUID, invalid body length or checksum mismatch
- **THEN** loading and inspection fail explicitly without accepting a partial asset

#### Scenario: Development output reset
- **WHEN** the storage change is adopted before the first formal release
- **THEN** old cook output is cleared and rebuilt rather than migrated or read through another format

### Requirement: Checked product saves report the actual publication result
SaveAsset and SaveProducts SHALL return explicit ProductStatus. A batch SHALL encode/validate and stage all products with required write, flush and readback checks before replacing destinations. Each destination replacement SHALL be atomic. A failure SHALL propagate through the builder and package flow; no failed batch SHALL be reported as a successful cook. A partial replacement or index failure SHALL invalidate the affected batch, and cleanup failures SHALL remain explicit errors. No guarantee of restoring a previous complete multi-product batch is made.

#### Scenario: Encoding or staging fails
- **WHEN** an input fails encoding, a staged write/flush fails, or readback does not match
- **THEN** the batch reports failure and does not replace any destination

#### Scenario: Failure after one destination was replaced
- **WHEN** a later atomic replacement or product index update fails
- **THEN** the affected batch is invalidated and cannot be accepted as successful package output
- **AND** the save does not claim rollback to a previous complete group

### Requirement: Loaded asset instances retain active data
A successful save SHALL allow subsequent loads to create a new immutable loaded instance with its own instance ID/revision. Existing referenced assets SHALL retain their loaded data and bound bulk bytes while in use; bulk reads SHALL NOT reopen a UUID path and silently select a newer file. Renderer/GPU borrowers SHALL keep their exact data through existing retirement protocols. ProductsOnly loads and InspectProduct SHALL not require source data or initiate cook. Inspection SHALL expose checked formatVersion/checksum without a runtime group.

#### Scenario: Reload with retained bulk readers
- **WHEN** a product file is replaced while a consumer retains a loaded asset
- **THEN** the old asset's data and bulk reads remain bound to its selected content
- **AND** a later load returns a distinct instance rather than mutating the retained object

#### Scenario: Products-only inspection
- **WHEN** a checked product is inspected with no source catalog
- **THEN** type, dependencies, format version, checksum and payload are read from the product without loading dependencies or mutating the asset cache
