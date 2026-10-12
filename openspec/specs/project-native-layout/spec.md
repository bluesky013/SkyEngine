# project-native-layout Specification

## Purpose
TBD - created by archiving change add-project-native-layout. Update Purpose after archive.
## Requirements
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

