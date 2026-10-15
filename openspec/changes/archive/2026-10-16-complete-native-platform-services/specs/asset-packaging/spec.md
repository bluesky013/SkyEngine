## MODIFIED Requirements

### Requirement: Cooked bundles are placed at the platform runtime location
Packaging SHALL place each selected bundle at the location the runtime bundle filesystem reads:
- Android: inside the APK under `assets/products/<bundle>/`.
- iOS: inside `<name>.app/products/<bundle>/`, alongside the app executable.
- macOS app bundles: inside `<name>.app/Contents/MacOS/products/<bundle>/`.
- Non-bundled desktop launchers: under `products/<bundle>/` next to the executable.

#### Scenario: Android bundle in the APK
- **WHEN** an Android package is produced
- **THEN** the selected bundles SHALL be readable from the APK's `assets/products/<bundle>/` at runtime

#### Scenario: Desktop bundle next to the launcher
- **WHEN** a desktop/iOS package is produced
- **THEN** the selected bundles SHALL exist under `products/<bundle>/` relative to the runtime bundle filesystem root: the iOS app root, macOS `Contents/MacOS`, or the non-bundled desktop executable directory
