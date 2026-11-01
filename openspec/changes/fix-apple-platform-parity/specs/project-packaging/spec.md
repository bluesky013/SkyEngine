## ADDED Requirements

### Requirement: Apple export configuration and metadata match the selected preset

Apple generation/build/collection SHALL resolve one debug/release configuration from the preset and use it for the actual CMake build and artifact path. versionName/versionCode SHALL reach the exported plist and final bundle. Export metadata SHALL be generated separately from project-owned source plists; orientation and other source metadata SHALL be preserved.

#### Scenario: Export a debug package with non-default versions
- **WHEN** an Apple preset selects debug and custom versionName/versionCode
- **THEN** the selected build and collected bundle SHALL be Debug with those version fields, while the source plist remains unchanged

#### Scenario: A package from another configuration exists
- **WHEN** the selected configuration has no bundle but another configuration does
- **THEN** collection SHALL fail explicitly rather than collect the other configuration's stale bundle

### Requirement: Signed Apple distribution stages resources before signing

A separate Apple archive/export workflow SHALL consume the resolved project/preset, stage cooked resources before Xcode signs, and require explicit external signing/export settings. It SHALL validate command results and final artifact presence before reporting success. Post-sign collection SHALL NOT modify the signed bundle. The existing unsigned collector SHALL retain its explicit signing=off contract.

#### Scenario: Archive and export for an explicitly selected device
- **WHEN** valid external signing/export settings are supplied and installation is requested
- **THEN** the workflow SHALL create a signed Xcode archive, export the artifact, verify its signature and install only on the selected device, preserving diagnostics for any failed stage

#### Scenario: Export fails after a previous success
- **WHEN** a later archive/export command fails
- **THEN** stale prior deliverables SHALL NOT be reported or installed as the result of that run

## MODIFIED Requirements

### Requirement: Cooked bundles are placed at the platform runtime location

Packaging SHALL place each selected bundle at the location the runtime bundle filesystem reads:
- Android: inside the APK under `assets/products/<bundle>/`.
- iOS: inside `<name>.app/products/<bundle>/`, alongside the app executable.
- macOS app bundles: inside `<name>.app/Contents/Resources/products/<bundle>/`.
- Non-bundled desktop launchers: under `products/<bundle>/` next to the executable.

#### Scenario: Android bundle in the APK
- **WHEN** an Android package is produced
- **THEN** the selected bundles SHALL be readable from the APK's `assets/products/<bundle>/` at runtime

#### Scenario: Desktop bundle next to the launcher
- **WHEN** a desktop/iOS package is produced
- **THEN** the selected bundles SHALL exist under `products/<bundle>/` relative to the runtime bundle filesystem root: the iOS app root, macOS `Contents/Resources`, or the non-bundled desktop executable directory
