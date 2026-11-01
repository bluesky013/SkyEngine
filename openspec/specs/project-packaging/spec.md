# project-packaging Specification

## Purpose

Define project-owned native launchers, export presets and deployment of cooked runtime resources.

## Scope notes

### export-preset

The distribution **export preset** (`export_presets.json`) is the shared contract between the editor (which edits
it through the reflected form) and the packaging CLI (`python/package_project.py`, which consumes it). This
capability defines that schema, its editor reflection surface, and the conformance guarantee that both consumers
derive from a single manifest (`configs/export_preset.schema.json`) and a single C++ reflection registration.

## Requirements

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

### Requirement: Single export preset schema manifest

There SHALL be exactly one machine-readable manifest listing the export preset field keys, checked into the
repository. The C++ field set SHALL live in the reflection registration (`RegisterExportPresetReflection`); the C++
test SHALL assert the reflected member set equals the manifest, and the packaging CLI SHALL be validated against the
manifest. Neither consumer SHALL define the key set independently.

#### Scenario: Manifest lists the keys

- **WHEN** the manifest is read
- **THEN** it SHALL list every key that appears in `export_presets.json`

#### Scenario: Located without depending on cwd

- **WHEN** the C++ conformance test runs from any working directory
- **THEN** it SHALL resolve the manifest path from a build-provided location, not from the process cwd

### Requirement: Reflection-driven export preset serialization

`ExportPreset` and `ExportPresets` SHALL serialize through custom `JsonSave`/`JsonLoad` registered with the type and
driven by the reflected members, in the flat `export_presets.json` format (a bare array of preset objects, or an
object with a `presets` array). Adding a field SHALL require only registering it - no separate loader/writer edit.

#### Scenario: Adding a field touches one list

- **WHEN** a field is added to the export preset
- **THEN** it SHALL be declared once in the reflection registration and listed in the manifest, and both load and save SHALL pick it up

#### Scenario: cacheVariables stays an object

- **WHEN** a preset is saved
- **THEN** `cacheVariables` SHALL be written as a JSON object, not an array

### Requirement: Editor models and round-trips every export preset field

`sky::ExportPreset` SHALL model and round-trip every manifest field, including the previously CLI-only
`cacheVariables`, `compileDefinitions`, `compileFlags`, `extraConfigureArgs`, and `extraBuildArgs`. Loading a preset
that contains these fields and saving it again SHALL preserve their values.

#### Scenario: Previously dropped fields are preserved

- **WHEN** a preset containing `cacheVariables`, `compileDefinitions`, `compileFlags`, `extraConfigureArgs`, and
  `extraBuildArgs` is loaded and saved by the editor model
- **THEN** all five fields SHALL be present with their original values in the saved output

#### Scenario: cacheVariables keeps its on-disk object shape

- **WHEN** a preset with `"cacheVariables": { "KEY": "VALUE" }` is loaded and saved
- **THEN** the saved `cacheVariables` SHALL remain a JSON object with the same key/value entries

### Requirement: Export preset fields are editable through the reflected form

Every export preset field SHALL be editable through the reflected form without hand-written per-field widgets.
`cacheVariables` SHALL be edited as an ordered key/value sequence.

#### Scenario: Editing a previously CLI-only field

- **WHEN** the user edits `cacheVariables` (add/remove/change an entry) in the export presets panel
- **THEN** the edit SHALL go through the reflected form and `CommandService` and be persisted on save

### Requirement: Schema conformance enforced in both consumers

A drift between the manifest and either consumer SHALL fail an automated test.

#### Scenario: C++ reflected key set matches the manifest

- **WHEN** the C++ conformance test compares the reflected members of `ExportPreset` to the manifest
- **THEN** the two key sets SHALL be equal, and a mismatch SHALL fail the test

#### Scenario: CLI key set matches the manifest

- **WHEN** the packaging CLI's preset field accesses are checked
- **THEN** every preset key literal referenced by `python/package_project.py` SHALL exist in the manifest, and any
  unknown key SHALL fail the Python conformance test

### Requirement: On-disk export preset format stays compatible

This change SHALL NOT change the on-disk `export_presets.json` schema for existing fields; existing files SHALL
continue to load and their keys SHALL remain valid for the packaging CLI.

#### Scenario: Existing preset still loads and packages

- **WHEN** an existing `export_presets.json` written before this change is opened by the editor and consumed by the
  CLI
- **THEN** it SHALL load without migration and the CLI SHALL read the same keys as before

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

### Requirement: Project native codebase with an independent engine entry

A project SHALL keep a single `native/` native codebase and consume the engine through an **independent CMake entry**
that adds the engine and plugin targets, rather than adding the engine's root CMake project.

#### Scenario: Entry does not run the engine root project
- **WHEN** the project's `native/CMakeLists.txt` is configured
- **THEN** it SHALL include the engine `cmake/*` helpers and `add_subdirectory` the engine and plugins targets, and it SHALL NOT configure the engine's root project (no engine `output/bin` override, no engine `tools/`)

#### Scenario: Native layout
- **WHEN** a project is created
- **THEN** it SHALL contain `native/` with the game module, the platform launcher entry, and the build entry (`CMakeLists.txt` / `CMakePresets.json`)

### Requirement: Project-owned launcher

The engine SHALL NOT ship a runnable launcher. The launcher target SHALL be named `Launcher` and SHALL be defined by
the project, so existing `DEPENDENCIES Launcher` registrations continue to bind to it.

#### Scenario: Engine has no launcher target
- **WHEN** the engine is configured for an engine-only build
- **THEN** it SHALL NOT define a `Launcher` executable target

#### Scenario: Project defines Launcher and its dependents build
- **WHEN** the project defines the `Launcher` target and calls `sky_set_dependency(TARGET Launcher)`
- **THEN** every target registered as `DEPENDENCIES Launcher` (engine, plugins, tools) SHALL be built and deployed alongside it

### Requirement: Artifact name is the project identifier

The built launcher artifact SHALL be named after the project identifier on every platform, independent of the CMake
target name.

#### Scenario: Per-platform artifact names
- **WHEN** the project launcher is built for a platform
- **THEN** the artifact SHALL be `<Name>.exe` (Windows), `<Name>` / `<Name>.app` (macOS per preset), and `lib<Name>.so` (Android)

#### Scenario: Android library name lock-step
- **WHEN** the Android launcher library is named `lib<Name>.so`
- **THEN** the Java activity `loadLibrary` and the manifest `android.app.lib_name` SHALL use the same `<Name>` (tokenized)

### Requirement: Per-platform build outputs under build/<platform>

Generated build/IDE projects and packaging artifacts SHALL live under `<Project>/build/<platform>/`, and output
directories SHALL be per-platform-correct.

#### Scenario: Generated project location
- **WHEN** an IDE/build project is generated for a platform
- **THEN** it SHALL be created under `<Project>/build/<platform>/`

#### Scenario: Android output directory not overridden
- **WHEN** the project is configured for Android
- **THEN** the build SHALL NOT override `CMAKE_*_OUTPUT_DIRECTORY` (so gradle's `externalNativeBuild` collects the native libraries)

### Requirement: CLI-authoritative project creation

Project creation SHALL be implemented by the packaging CLI, and the editor SHALL invoke that CLI rather than
re-implementing scaffolding.

#### Scenario: Create via CLI
- **WHEN** a project is created
- **THEN** `python/package_project.py create` SHALL be the implementation that scaffolds `native/` and writes the descriptor

#### Scenario: Editor shells out
- **WHEN** the editor's Project Manager creates a project and Python is available
- **THEN** it SHALL invoke the CLI; WHEN Python is unavailable it SHALL report a clear error and not crash

### Requirement: Plugin compile-selection and config deployment

Engine plugins SHALL be selected at compile time via build options, and the project launcher SHALL deploy the engine
and project `configs/`.

#### Scenario: Plugin selection
- **WHEN** the project is configured
- **THEN** plugins SHALL be included per `SKY_BUILD_<x>` (defaults from `plugins/plugins.json`, overridable per preset), and editor-only plugins SHALL be excluded from a project build

#### Scenario: Config deployment order
- **WHEN** the project launcher artifacts are produced
- **THEN** the engine `configs/` SHALL be deployed first and the project `configs/` second, so the project's `modules_game.json` overrides

### Requirement: World assets use checked binary products

The existing asset pipeline SHALL cook `.world` authoring JSON into versioned typed binary World products. Actor identity, hierarchy, component persistence Data, typed subsystem configs and asset dependencies SHALL survive roundtrip. Runtime SHALL load World through the existing product index with ProductsOnly. The decoder SHALL reject unsupported schemas, truncation, invalid counts/lengths, duplicate identities and invalid hierarchy before publishing a partial World.

#### Scenario: Cook and run the default World
- **WHEN** a project selects a valid World with Cube, Plane, camera and lights as defaultScene
- **THEN** the public package flow produces a binary World asset, audits its dependency closure and automatically starts that World using the ordinary game template
- **AND** no authoring World JSON is shipped

#### Scenario: Preserve logical symlink identity
- **WHEN** a selected source path aliases another file inside project assets
- **THEN** its logical source/product path remains the selected path
- **AND** a symlink escaping project assets fails validation

#### Scenario: Android collects checked flat products
- **WHEN** an Android APK collects cooked asset products
- **THEN** the selected product index and its referenced UUID-addressed checked product files are packaged together using default asset filters
- **AND** the default World loads from binary products without source access

### Requirement: SPIR-V products omit optional source declarations

The compiler SHALL omit `OpSource` and `OpSourceContinued` from finalized SPIR-V artifacts, retaining all other instructions, native entry identity and reflection. The policy SHALL participate in the SPIR-V producer identity. Malformed instruction boundaries or entry declarations SHALL fail without publishing partial code.

#### Scenario: Older Vulkan source metadata parser
- **WHEN** Slang emits a source-language declaration that an older software Vulkan parser does not recognize
- **THEN** the cooked module contains no optional source declarations and its executable shader semantics remain unchanged

### Requirement: World simulation is independent of presentation

The application SHALL tick the active World before scene extraction on active application frames even when renderer submission is BUSY. The launcher host SHALL own packet sequence/submission, scene/view/output identities and main view revisions.

#### Scenario: Renderer backpressure
- **WHEN** a pending render packet remains BUSY
- **THEN** application simulation continues while the pending render snapshot stays immutable

### Requirement: Output transforms and completed prepare ownership are single-use

Runtime SHALL preserve logical camera projection and let frame composition apply the output transform once. Task drain callback transfer SHALL empty the original owner while retaining callback code/state through the invocation. Completed prepare products SHALL be released after their consumers and active-group ownership retire.

#### Scenario: Rotated Vulkan output
- **WHEN** an Android surface reports a quarter-turn output transform
- **THEN** the logical camera is preserved and the output mapping is applied once without geometry distortion

#### Scenario: Small drain callback retains a job
- **WHEN** a completed prepare job uses a small callable with shared state under libc++
- **THEN** draining clears the original callback and retires the previous products without a self-reference cycle

### Requirement: Launcher scene sessions have explicit ownership and events

A normally initialized launcher SHALL expose one session registration using an owned session and exact host/generation token; provider-only mode SHALL NOT. Sessions SHALL receive Append context and relevant RendererEvents from the unique poll owner. Replacement SHALL wait for retirement of the previous scene generation. Failed or stale unregister operations SHALL NOT remove another session.

#### Scenario: Dynamic entity churn
- **WHEN** a session repeatedly creates and removes entities
- **THEN** exact EntityDestroyed events reach its Bridge and retired slots are reused

#### Scenario: Session replacement
- **WHEN** an executing session unregisters or is replaced
- **THEN** its callback object survives the current invocation while defining DLL code remains process-resident, and the replacement uses a new retired-safe scene generation

### Requirement: Native iOS launch and reproducible fixture

The iOS launcher SHALL include a launch-screen declaration. The reusable scene fixture SHALL use the ordinary project template and parameterized simulator SDK/architecture/dependency paths.

#### Scenario: Modern iPhone canvas
- **WHEN** the configured app runs on iPhone 16 Pro / iOS 18.6
- **THEN** portrait view/framebuffer sizes are 402x874 points / 1206x2622 pixels
