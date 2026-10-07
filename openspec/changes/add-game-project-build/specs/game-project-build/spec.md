# Delta: game-project-build

## ADDED Requirements

### Requirement: Project scaffolding generates a game module project

Creating a project SHALL generate, in addition to the existing `assets/`/`configs/`/`cache/`/`<name>.skyproj`, a
`source/` tree containing a Game module skeleton and a project-level build entry, so the project's game code is
compilable on every supported platform. The scaffolded files SHALL come from a single engine template
(`engine/templates/game/`).

#### Scenario: New project has a source tree

- **WHEN** a new project `TestProject` is created
- **THEN** `TestProject/source/` SHALL contain a `<Name>Module` module (headers, sources, `CMakeLists.txt`) and `TestProject/` SHALL contain a build entry (`CMakeLists.txt` / `CMakePresets.json`)

#### Scenario: Module is registered

- **WHEN** the scaffold is generated
- **THEN** the `<name>.skyproj` `modules` list and the project `configs/modules_game.json` SHALL include the game module id

### Requirement: Game module builds cross-platform as a loadable shared module

The generated game module SHALL build as a SHARED library exporting `StartModule`/`StopModule` (via
`REGISTER_MODULE`) and SHALL be loadable by `ModuleManager`. The project build SHALL reuse the engine's CMake
helpers and third-party flow (`3RD_PATH`) and SHALL support the engine's platform set.

#### Scenario: Build a module per platform

- **WHEN** the project is configured/built for a supported platform (Win32/macOS/Linux/Android)
- **THEN** it SHALL produce the game module as that platform's shared library

#### Scenario: Runtime load

- **WHEN** the launcher starts with the project
- **THEN** `ModuleManager` SHALL load the game module by name and the module's `Tick` SHALL run

#### Scenario: Engine reference

- **WHEN** the project build resolves the engine
- **THEN** it SHALL use the engine source tree via `SKY_ENGINE_ROOT` (initial `add_subdirectory` approach)

### Requirement: Packaging ships cooked products, not source assets

A shipped project SHALL package the **cooked product bundles** for the target platform (`product.index` +
`<uuid>.bin` payloads), not the raw `assets/` source files. The runtime SHALL load product bundles (the engine
`common`/platform bundles plus the project's cooked bundle); it SHALL NOT require the raw `assets/` directory.
The `--project <dir>` dev mode (loading from the project tree, including on-demand cook in the editor) is a
development convenience and is not the shipping layout.

#### Scenario: Shipped layout contains products

- **WHEN** a project is packaged for a platform
- **THEN** the output SHALL contain that platform's product bundle(s) (cooked), and SHALL NOT ship the source `assets/`

#### Scenario: Runtime reads products

- **WHEN** the shipped game runs
- **THEN** `AssetManager` SHALL resolve assets from product bundles only (no source catalog)

#### Scenario: Dev mode differs

- **WHEN** the game runs via `--project <dir>` from the project tree
- **THEN** it MAY read the project's `assets/` (and on-demand cook in the editor); this is not the shipping layout

### Requirement: Configurable bundle id, signing, and IDE project generation

Project packaging SHALL support a configurable application identity and signing, and SHALL be able to generate
the platform IDE project from the same project configuration:

- **Bundle id**: a project setting maps to Android `applicationId` (and gradle `namespace`) and to Apple
  `CFBundleIdentifier`; the value SHALL flow from the project descriptor/settings, not be hardcoded.
- **Signing**: Android keystore (path/alias/passwords, kept out of VCS) and Apple signing
  (`DEVELOPMENT_TEAM`, provisioning, entitlements/codesign) SHALL be configurable per project.
- **IDE project generation**: the project SHALL generate an **Android Studio** gradle project and an **Xcode**
  project (iOS/macOS) carrying the resolved id/signing, so the game can be opened/run from the IDE.
- The same project configuration SHALL drive build -> IDE project generation -> signing -> output (`APK` / `.app`).

#### Scenario: Custom bundle id

- **WHEN** a project sets its application id
- **THEN** the generated Android `applicationId`/`namespace` and Apple `CFBundleIdentifier` SHALL use that value

#### Scenario: Signing configured

- **WHEN** signing is configured for a platform
- **THEN** packaging SHALL sign the output with the configured identity (Android keystore / Apple team+provisioning)

#### Scenario: Generate IDE projects

- **WHEN** IDE project generation is requested
- **THEN** an Android Studio gradle project and an Xcode project SHALL be produced with the resolved id/signing

#### Scenario: End-to-end packaging

- **WHEN** a project is packaged for a platform
- **THEN** one configuration SHALL drive build, IDE project generation, signing, and the final `APK`/`.app`


