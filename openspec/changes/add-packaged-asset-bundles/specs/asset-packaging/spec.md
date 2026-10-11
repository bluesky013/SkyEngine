## ADDED Requirements

### Requirement: Per-distribution bundle selection
Product-bundle selection SHALL be a property of the distribution (export preset), so the same platform can ship
several packages (e.g. Android normal and HD) with different bundle sets.

#### Scenario: Two distributions of one platform
- **WHEN** two presets target `android` with different bundle sets
- **THEN** each packaged APK SHALL contain only that preset's selected bundles

#### Scenario: Explicit override
- **WHEN** an export preset lists an explicit `bundles` set
- **THEN** packaging SHALL use it, otherwise falling back to the cook config's distribution bundles

### Requirement: Quality tiers use distinct bundle names
Quality tiers SHALL be represented as distinct bundle names, so a shipped package never contains two bundles that map
the same asset UUID (which would make first-match resolution ambiguous).

#### Scenario: Low vs HD
- **WHEN** a texture is cooked into `tex_low` and `tex_hd` for two distributions
- **THEN** each package SHALL contain exactly one of those bundles

### Requirement: Cooked bundles are placed at the platform runtime location
Packaging SHALL place each selected bundle at the location the runtime reads:
- Android: inside the APK under `assets/products/<bundle>/`
- desktop / iOS: `products/<bundle>/` next to the launcher (inside a `.app`, `Contents/Resources/products/`)

#### Scenario: Android bundle in the APK
- **WHEN** an Android package is produced
- **THEN** the selected bundles SHALL be readable from the APK's `assets/products/<bundle>/` at runtime

#### Scenario: Desktop bundle next to the launcher
- **WHEN** a desktop/iOS package is produced
- **THEN** the selected bundles SHALL exist under `products/<bundle>/` reachable from the launcher's bundle path

### Requirement: Package ships a bundles manifest
A package SHALL contain a `products/bundles.json` manifest, written by packaging, listing the included bundles (and
the platform / distribution it was built for).

#### Scenario: Manifest lists the packaged bundles
- **WHEN** a package is produced
- **THEN** `products/bundles.json` SHALL list exactly the bundles copied into the package

### Requirement: Runtime mounts exactly the manifest's bundles
The runtime SHALL read `products/bundles.json` and add exactly the listed bundles through
`AssetManager::AddAssetProductBundle` (no filesystem enumeration); when the manifest is missing it SHALL log and fall
back to `common`.

#### Scenario: Mount from manifest
- **WHEN** the runtime starts from a package whose `products/bundles.json` lists `["common","tex_hd"]`
- **THEN** it SHALL mount `common` and `tex_hd` (and only those)

#### Scenario: Missing manifest
- **WHEN** `products/bundles.json` is absent
- **THEN** the runtime SHALL log a warning and mount `common` without crashing
