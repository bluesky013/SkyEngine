## ADDED Requirements

### Requirement: Per-directory source manifest

The system SHALL persist source-asset identity in a per-directory manifest named `assets.jsonl` in every directory that contains registered source assets.

The manifest SHALL be UTF-8 with LF line endings and use **JSON Lines**: one JSON object per line, `{"file":"<filename>","id":"<uuid>"}`, optionally carrying a `"cook"` block (the asset cook config; its semantics are defined by the `asset-cook-config` capability). Lines MUST be sorted by `file` so output is deterministic and multi-branch merges stay line-based. The manifest SHALL be committed to version control in each writable source mount.

#### Scenario: Manifest created on first registration

- **WHEN** a source asset in a directory with no manifest is registered
- **THEN** the system SHALL create `assets.jsonl` in that directory containing a line with the new asset's filename and UUID

#### Scenario: Deterministic sorted output

- **WHEN** a directory manifest is written after adding or moving entries
- **THEN** its entries SHALL be ordered by filename and the file SHALL end with a newline

### Requirement: Stable one-time UUID assignment

Registering a source asset SHALL resolve its UUID from the directory manifest if an entry exists, otherwise SHALL generate a new UUID and persist it. After migration, the UUID of a source asset MUST NOT be recomputed from its path.

#### Scenario: Repeated registration is stable

- **WHEN** the same source path is registered more than once
- **THEN** the system SHALL return the same UUID each time

#### Scenario: Manifest entry is authoritative

- **WHEN** an asset already has a manifest entry and is registered again without a manifest change
- **THEN** the system SHALL return the manifest UUID and MUST NOT derive a new UUID from the path

### Requirement: Migration preserves existing references

Before identity assignment stops deriving from paths, a one-time migration SHALL scan the source mounts (filtered by builder-known extensions, including world files) and seed each manifest entry with its legacy path-derived UUID, computed from the asset's mount/legacy bundle role to match the old `(bundle, path)` scheme, so that references serialized by earlier builds continue to resolve. `assets.db`, when present, MAY be used as a cross-check but MUST NOT be required.

#### Scenario: Existing reference still resolves after migration

- **WHEN** a world or asset payload holds a UUID produced by the legacy path-derived scheme
- **THEN** after migration the resolver SHALL map that UUID to the same source asset

#### Scenario: Migration does not require assets.db

- **WHEN** migration runs on a fresh checkout with no `assets.db`
- **THEN** it SHALL still seed manifests from the scanned source tree and preserve existing references

### Requirement: Generated sources obtain identity through the resolver

Source documents whose identity was previously computed from their path (world documents) SHALL be registered as source assets and obtain their UUID through the same resolver, and migration SHALL cover them.

#### Scenario: World keeps its identity

- **WHEN** a world document resolves its identity after migration
- **THEN** its persist ID SHALL come from the resolver and match the pre-migration path-derived value

### Requirement: Source identity resolution

For identity and authoring (registration, build, mutation), the system SHALL resolve a source asset by path through the directory manifest (with a parsed-manifest cache) and by UUID through the identity map. Source identity resolution MUST NOT depend on `assets.db` being present. This is not an asset-data load path; asset data is always loaded from cooked products.

#### Scenario: Resolve by path

- **WHEN** `FindAsset(path)` is called for a migrated asset
- **THEN** the system SHALL return the source info whose UUID matches the directory manifest

#### Scenario: Resolve by UUID without assets.db

- **WHEN** `assets.db` is absent but directory manifests exist
- **THEN** resolving a known UUID SHALL still yield the correct source asset after the cache is rebuilt

### Requirement: Runtime lookup is independent of assets.db

The shipped runtime MUST NOT read `assets.db` or source `assets.jsonl` manifests. Runtime path-to-UUID lookup SHALL use a generated product index emitted into the product bundle; each index entry SHALL be a JSON Lines record `{"path","id"}` mapping a logical path to a UUID, and product payloads SHALL remain UUID-keyed.

#### Scenario: Runtime resolves a path without assets.db

- **WHEN** runtime calls `LoadAssetFromPath` in a build that ships no `assets.db`
- **THEN** the system SHALL resolve the UUID through the generated product index and load the product payload

#### Scenario: Runtime bundle contains no assets.db

- **WHEN** the runtime bundle is assembled
- **THEN** it SHALL contain the generated product index and MUST NOT contain `assets.db` or `assets.jsonl`

#### Scenario: Lookup spans multiple product bundles

- **WHEN** a runtime path→uuid lookup is made and products are split across bundles (e.g. textures in a platform bundle)
- **THEN** the resolver SHALL consult the merged indexes of all added product bundles

### Requirement: Mount-based logical namespace

Source roots SHALL be ordered mounts in the engine `MultiFileSystem` (workspace writable first, engine read-only next, with custom/pak/DLC adding more). Assets SHALL be keyed by a single logical path with no bundle field, and precedence SHALL be the mount order.

#### Scenario: Override by mount order

- **WHEN** the same logical path exists in two mounts
- **THEN** the earlier mount SHALL win

#### Scenario: Manifest from the owning mount

- **WHEN** a logical path is resolved
- **THEN** its UUID SHALL come from the `assets.jsonl` of the mount that owns the file

### Requirement: Single asset type identifier

The asset type SHALL be identified by one `AssetTypeId` shared by the type/handler registry and the product header. A source asset MUST NOT persist a separate type/category field.

#### Scenario: Source type is derived

- **WHEN** the system needs the type of a source asset
- **THEN** it SHALL derive it from the asset's extension through the builder registry, and SHALL NOT read a stored category field

#### Scenario: Product type comes from the header

- **WHEN** a product is loaded
- **THEN** the handler SHALL be selected by the `AssetTypeId` recorded in the product header

#### Scenario: No persisted category

- **WHEN** a manifest or dev-cache entry is written for a source asset
- **THEN** it SHALL contain no type/category field

### Requirement: Type identifier consistency

The `AssetTypeId` SHALL be the same identifier string across `AssetTraits<T>::ASSET_TYPE`, the builder registry's `QueryType(ext)`, the `AssetManager` handler-registry key, the product header `type`, and the editor property metadata (`SET_ASSET_TYPE`).

#### Scenario: Builder type resolves to a handler

- **WHEN** a builder reports a type for an extension
- **THEN** that type SHALL resolve to a registered handler and SHALL equal the type written into the product header

#### Scenario: Unknown builder type is rejected

- **WHEN** a builder reports a type for which no handler is registered
- **THEN** the registration or build SHALL be rejected rather than producing a product with an unloadable type

#### Scenario: Property type matches the asset type

- **WHEN** a property declares an asset type
- **THEN** that declared type SHALL equal the `AssetTypeId` of the assets it accepts, so type-based selection and validation agree with the handler and header type

### Requirement: Asset dependency provider

An `IAssetDependencyProvider` SHALL expose forward dependencies, reverse dependents, and full-graph iteration for assets.

#### Scenario: Forward dependencies

- **WHEN** the dependencies of an asset are queried
- **THEN** the provider SHALL return the direct dependencies recorded for that asset

#### Scenario: Reverse dependents

- **WHEN** the dependents of an asset are queried
- **THEN** the provider SHALL return the assets that reference it, computed by inverting the forward graph

### Requirement: Dependency data provenance

At runtime the forward graph SHALL be assembled from product headers; in the editor/build it SHALL be assembled from the source dependencies and persisted only as a derived cache. Dependencies SHALL NOT be stored in `assets.jsonl` or `product.index`.

#### Scenario: Runtime graph from product headers

- **WHEN** the runtime needs an asset's dependencies
- **THEN** it SHALL read them from the product header

#### Scenario: Editor graph not in the manifest or index

- **WHEN** the dependency graph is persisted in the editor
- **THEN** it SHALL live in the dev cache and MUST NOT appear in `assets.jsonl` or `product.index`

### Requirement: Dependency impact reporting

The provider SHALL allow tooling to query an asset's dependents before a destructive operation.

#### Scenario: Removing a referenced asset reports dependents

- **WHEN** an asset that other assets depend on is about to be deleted
- **THEN** the provider SHALL report its dependents so the operation can warn or be blocked
