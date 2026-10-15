# game-project-build Specification

## Purpose
TBD - created by archiving change add-game-project-build. Update Purpose after archive.
## Requirements
### Requirement: Project scaffolding generates a game module project

Creating a project SHALL generate, in addition to the existing `assets/`/`configs/`/`cache/`/`<name>.skyproj`, a
`native/` tree (the single native codebase) containing a Game module skeleton, the platform launcher entry, and an
independent project-level build entry, so the project's game code is compilable on every supported platform. The
scaffolded files SHALL come from the engine templates (`engine/templates/{game,launcher}/`).

#### Scenario: New project has a native tree

- **WHEN** a new project `TestProject` is created
- **THEN** `TestProject/native/source/` SHALL contain a `<Name>Module` module and `TestProject/native/` SHALL contain the build entry (`CMakeLists.txt` / `CMakePresets.json`) and launcher entry

#### Scenario: Module is registered

- **WHEN** the scaffold is generated
- **THEN** the `<name>.skyproj` `modules` list and the project `configs/modules_game.json` SHALL include the game module id

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

Project packaging SHALL support a configurable application identity and platform signing settings, and SHALL
generate the platform IDE project from the same project configuration:

- **Bundle id**: a project setting maps to Android `applicationId` (and Gradle `namespace`) and to Apple
  `CFBundleIdentifier`; the value SHALL flow from the project descriptor/settings, not be hardcoded.
- **Signing settings**: Android keystore (path/alias/passwords, kept out of VCS) and Apple team, signing identity
  and provisioning profile SHALL be configurable per project and forwarded into the generated platform build.
- **IDE project generation**: the project SHALL generate an Android Studio Gradle project and an Xcode
  project (iOS/macOS) carrying the resolved identity and signing settings, so the game can be opened/run from the IDE.
- **Apple package collection**: the public package command SHALL require explicit `signing=off` before building
  or modifying an Apple bundle. It SHALL collect an unsigned iOS IPA or macOS app ZIP after staging resources;
  it SHALL NOT present this collector as a signed Xcode archive/export or notarization workflow.
- The same project configuration SHALL drive IDE project generation, native build and the supported package output.

#### Scenario: Custom bundle id

- **WHEN** a project sets its application id
- **THEN** the generated Android `applicationId`/`namespace` and Apple `CFBundleIdentifier` SHALL use that value

#### Scenario: Signing configured

- **WHEN** signing settings are configured for platform project generation
- **THEN** the generated build SHALL receive those settings; Android release packaging SHALL use its configured keystore, while Apple package collection with signing enabled SHALL fail before building or changing the bundle and direct signed distribution to a separate Xcode archive/export workflow

#### Scenario: Generate IDE projects

- **WHEN** IDE project generation is requested
- **THEN** an Android Studio Gradle project and an Xcode project SHALL be produced with the resolved identity and signing settings

#### Scenario: End-to-end packaging

- **WHEN** a project is packaged for a supported platform
- **THEN** one configuration SHALL drive the native build and staged resources into the selected Android APK or, with Apple signing disabled, `Payload/<name>.app` inside an unsigned IPA or `<name>.app` inside a macOS ZIP; signed Apple archive/export SHALL remain a separate distribution step

### Requirement: Game module builds cross-platform with preset-selected packing

The generated game module SHALL use `sky_add_module` and `REGISTER_MODULE_TAGGED` so its packaging follows
`SKY_MODULE_PACKAGING`. Dynamic mode SHALL produce a SHARED library exporting `StartModule`/`StopModule`;
static mode SHALL produce a STATIC library with uniquely tagged factories linked into and registered by the host.
The generated iOS export preset SHALL select static packaging. Both modes SHALL make the game module available
to `ModuleManager` under the same module id. The project build SHALL reuse the engine's CMake helpers and
third-party flow (`3RD_PATH`) and SHALL support the engine's platform set.

#### Scenario: Build a module per platform

- **WHEN** the project is configured/built for a supported platform with its selected module packaging mode
- **THEN** dynamic mode SHALL produce the platform's shared game module, and static mode SHALL link and register the game module in the launcher without requiring a separate shared game-module library

#### Scenario: Runtime load

- **WHEN** the launcher starts with the project
- **THEN** `ModuleManager` SHALL resolve the game module by name through dynamic loading or static registration, and the module's `Tick` SHALL run

#### Scenario: Engine reference

- **WHEN** the project build resolves the engine
- **THEN** it SHALL consume the engine through an independent CMake entry (`native/CMakeLists.txt` includes the engine `cmake/*` helpers and adds the engine + plugins targets)

