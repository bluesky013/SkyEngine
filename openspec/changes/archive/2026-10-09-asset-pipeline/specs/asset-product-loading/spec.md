## ADDED Requirements

### Requirement: Loading is always from cooked products

The editor and the runtime SHALL load asset data exclusively from cooked products through the same product-based loader (`AssetManager`) and the same product path-to-UUID index. Neither the editor nor the runtime SHALL load asset data from source files. The source catalog (`assets.jsonl` / `assets.db`) MUST NOT be used to obtain asset data; it MAY only be queried to resolve a path/id, check existence, or select a cook target — never for asset data.

#### Scenario: Editor loads products

- **WHEN** an editor consumer requests an asset
- **THEN** it SHALL load the cooked product payload through `AssetManager`, exactly as the runtime does

#### Scenario: Editor load reads no source

- **WHEN** the editor loads an asset
- **THEN** it SHALL read only cooked products and the product index, and MUST NOT read the source file or the source manifest for asset data

### Requirement: Product payloads reference assets by UUID only

A product payload that references another asset SHALL store a UUID and MUST NOT store a source-relative path, so the loader never consults the source catalog for data.

#### Scenario: Material payload uses UUID references

- **WHEN** a material product references a texture
- **THEN** the payload SHALL contain the texture's UUID, and loading SHALL NOT resolve a source path

### Requirement: Path resolution has an editor-only source fallback

When a load key is a path, the loader SHALL resolve it through the product index first; on a miss with the editor source catalog present, it SHALL resolve through the source catalog; if both miss, the asset SHALL be reported missing. The runtime SHALL resolve paths only through the product index.

#### Scenario: Built asset resolves via the product index

- **WHEN** a path load matches an entry in the product index
- **THEN** the loader SHALL resolve the UUID from the product index without consulting the source catalog

#### Scenario: Uncooked source resolves via the editor fallback

- **WHEN** a path load misses the product index but the editor source catalog can resolve the path
- **THEN** the loader SHALL obtain the UUID from the source catalog and proceed to the on-demand cook branch

#### Scenario: Runtime path miss reports missing

- **WHEN** the runtime path load misses the product index
- **THEN** the load SHALL fail, since the runtime has no source catalog

### Requirement: On-demand cook when a product is missing

When a load request finds no product, the loader SHALL consult source existence: if a source record exists it SHALL schedule one cook (coalesced per UUID) and return the asset in a **LOADING** state, becoming **LOADED** on cook success or **FAILED** on cook failure; if no source record exists it SHALL report the asset as missing. Source existence SHALL mean an `AssetDataBase` record for the UUID, not the presence of the `assets.db` file. In the runtime the source locator is empty, so a missing product SHALL be a hard error.

#### Scenario: Missing product with a source triggers a cook

- **WHEN** the loader finds no product but a source record exists
- **THEN** it SHALL schedule a cook and return the asset in a LOADING state

#### Scenario: Cook target from the effective cook configuration

- **WHEN** a cook is scheduled
- **THEN** the target bundle SHALL be chosen from the effective cook configuration (asset override × project preset × current platform), defaulting to the primary bundle

#### Scenario: Missing product without a source reports missing

- **WHEN** the loader finds no product and no source record exists
- **THEN** it SHALL report the asset as missing

#### Scenario: Runtime missing product errors

- **WHEN** the runtime loads an asset with no product
- **THEN** the load SHALL fail, and no cook SHALL be attempted

#### Scenario: Cook failure leaves the asset FAILED

- **WHEN** a scheduled cook fails or no builder is available for the source
- **THEN** the asset SHALL become FAILED (not remain LOADING), MUST NOT fall back to the source, and a later load SHALL re-attempt

#### Scenario: Dependencies trigger their own cooks

- **WHEN** an asset's product exists but a dependency's product is missing with an available source
- **THEN** the dependency's load SHALL schedule its own cook and the dependent asset SHALL wait for the dependency chain

### Requirement: Cook completion is reported as an event

Cook completion SHALL be reported through an asset event keyed by the asset UUID, carrying success or failure, so a LOADING asset can transition and the caller can react. Both cook modes SHALL raise the same event.

#### Scenario: Success completes the pending asset

- **WHEN** a cook finishes successfully
- **THEN** the build-finished event SHALL be raised for the asset UUID and the asset SHALL become LOADED

#### Scenario: Failure leaves the asset FAILED

- **WHEN** a cook finishes with a failure
- **THEN** the event SHALL carry the failure and the asset SHALL become FAILED

### Requirement: Cook supports in-process and out-of-process runners

The system SHALL support cooking in-process (the same process as the editor) or out-of-process (a separate process), selectable by configuration. The loading layer SHALL depend only on the cook-completion event and MUST NOT depend on the cook mode.

#### Scenario: In-process cook completes in-process

- **WHEN** the in-process runner is configured and a cook is scheduled
- **THEN** the build SHALL run on a cook pool (not the loader pool) and raise the completion event in the same process

#### Scenario: Out-of-process cook raises the same event

- **WHEN** the out-of-process runner is configured and a cook is scheduled
- **THEN** the build SHALL run in a separate process and, on completion, the same in-process completion event SHALL be raised

### Requirement: Concurrency safety for load and cook

Loading and cooking SHALL not deadlock or lose state: an in-process cook MUST NOT occupy a loader worker while waiting, `product.index` writes SHALL be serialized per bundle, and a LOADING asset's wait handle SHALL be established before its cook is scheduled.

#### Scenario: No loader-pool starvation

- **WHEN** an on-demand cook is scheduled from a load running on the asset pool
- **THEN** the cook SHALL run without occupying a loader worker, so the pool cannot deadlock

#### Scenario: Concurrent index updates do not lose entries

- **WHEN** multiple cooks complete for the same bundle
- **THEN** each `product.index` update SHALL be serialized so no entry is lost

#### Scenario: Wait handle ready before cook

- **WHEN** a load returns a LOADING asset
- **THEN** its wait handle SHALL already be established so a blocking wait unblocks on LOADED or FAILED
