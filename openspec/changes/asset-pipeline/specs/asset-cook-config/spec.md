## ADDED Requirements

### Requirement: Source import flow

Importing a source asset SHALL copy it into the writable mount (workspace), assign it a stable identity (a manifest entry), and create its asset-level cook-config entry. Import MUST NOT modify any asset references, and MAY cook the current platform immediately (configurable, default on).

#### Scenario: Import assigns identity

- **WHEN** a source file is imported into the workspace
- **THEN** it SHALL receive a manifest identity and an asset-level cook-config entry, and no reference SHALL change

#### Scenario: Import cooks the current platform

- **WHEN** import is configured to cook immediately
- **THEN** the system SHALL schedule a cook for the current platform's target(s)

### Requirement: Project and asset cook configuration

A project-level cook config SHALL define platform-to-target presets and per-asset-kind settings (encoder, mips, compression). Each source asset SHALL be able to override these via the `cook` block of its line in the per-directory `assets.jsonl` manifest. The effective settings SHALL be the asset override applied over the project preset for the current platform. Both the project cook config and the asset manifest SHALL be human-editable text.

#### Scenario: Asset override wins

- **WHEN** an asset-level config sets a value that the project preset also sets
- **THEN** the asset-level value SHALL be used for that asset

#### Scenario: Project default applies

- **WHEN** an asset has no override for a setting
- **THEN** the project preset value for the current platform SHALL apply

### Requirement: Per-asset user cook config

An asset's `cook` block SHALL support a free-form `user` object for asset-specific builder parameters; the effective cook configuration SHALL expose it to the builder.

#### Scenario: User parameters reach the builder

- **WHEN** an asset's `cook.user` sets a builder-specific parameter
- **THEN** the effective cook configuration passed to the builder SHALL include it

### Requirement: Multi-platform target outputs

A source asset configured for several targets SHALL emit one product per target; all products SHALL share the source UUID, each SHALL live in its target bundle, and each bundle's product index SHALL map the logical path to that UUID.

#### Scenario: One texture, two platform products

- **WHEN** a texture is configured for both a BC target and an ASTC target
- **THEN** two products SHALL be emitted under the same UUID, one per target bundle

### Requirement: Cook data compression

Cook MAY compress product data. The product header SHALL record the compression codec, and the loader SHALL decompress via the framework `CompressionManager` (`ICompressor`), whose lz4 implementation is registered by the dynamically loaded `CompressionModule`.

#### Scenario: Compressed product round-trips

- **WHEN** a target is configured to compress and the codec module is loaded
- **THEN** the loader SHALL decompress the product before deserialization

#### Scenario: Uncompressed needs no codec

- **WHEN** a product is not compressed
- **THEN** loading SHALL NOT require any compression module

### Requirement: Cook worker protocol integrity

The out-of-process cook worker SHALL frame its protocol on stdout with a defined length-prefixed encoding and SHALL send logs to stderr so frames are never corrupted; it SHALL start with the same mount namespace and platform target as the editor.

#### Scenario: Logs do not corrupt the protocol

- **WHEN** the worker writes logs while serving requests
- **THEN** logs SHALL go to stderr and stdout SHALL carry only protocol frames

#### Scenario: Worker shares the mount namespace

- **WHEN** the worker resolves a source path
- **THEN** it SHALL use the same mounts and platform target as the editor
