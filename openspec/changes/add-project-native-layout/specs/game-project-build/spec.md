# Delta: game-project-build

## MODIFIED Requirements

### Requirement: Project scaffolding generates a game module project

Creating a project SHALL generate, in addition to the existing `assets/`/`configs/`/`cache/`/`<name>.skyproj`, a
`native/` tree (the single native codebase) containing a Game module skeleton, the platform launcher entry, and an
independent project-level build entry, so the project's game code is compilable on every supported platform. The
scaffolded files SHALL come from engine templates (`engine/templates/{game,launcher}/`).

#### Scenario: New project has a native tree

- **WHEN** a new project `TestProject` is created
- **THEN** `TestProject/native/source/` SHALL contain a `<Name>Module` module (headers, sources, `CMakeLists.txt`), `TestProject/native/` SHALL contain the build entry (`CMakeLists.txt` / `CMakePresets.json`) and the platform launcher entry, and generated build projects SHALL live under `TestProject/build/<platform>/`

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
- **THEN** it SHALL consume the engine source tree through an independent CMake entry (`native/CMakeLists.txt` includes the engine `cmake/*` helpers and adds the engine + plugins targets), not by adding the engine's root project
