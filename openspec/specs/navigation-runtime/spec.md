# navigation-runtime Specification

## Purpose
TBD - created by archiving change fix-navigation-recast. Update Purpose after archive.
## Requirements
### Requirement: Recast backend is listed in runtime module configs

`RecastNavigation` SHALL be listed in the game and editor runtime module configurations so `ModuleManager` loads
it, mirroring `BulletPhysicsModule`. The build switch remains `SKY_BUILD_RECAST`.

#### Scenario: Game runtime loads the recast backend

- **WHEN** the game application starts with `RecastNavigation` listed in `configs/modules_game.json`
- **THEN** the module SHALL be loaded and `NaviMeshFactory` SHALL have a registered implementation

#### Scenario: Editor runtime loads the recast backend

- **WHEN** the editor starts with `RecastNavigation` listed in `configs/modules_editor.json`
- **THEN** the module SHALL be loaded and `NaviMeshFactory` SHALL have a registered implementation

#### Scenario: Backend disabled

- **WHEN** `SKY_BUILD_RECAST` is OFF
- **THEN** `RecastNavigation` SHALL NOT be built and navigation SHALL remain in its null-backend state

### Requirement: Factory register/unregister lifecycle

`NaviMeshFactory` SHALL expose `UnRegister()` that clears the registered implementation, and the recast module
SHALL register on `Start()` and unregister on `Shutdown()` so the implementation never outlives its module.

#### Scenario: Module shutdown clears the factory

- **WHEN** `RecastModule::Shutdown` runs
- **THEN** `NaviMeshFactory::CreateNaviMesh` SHALL return `nullptr`

#### Scenario: Load and unload cycle is safe

- **WHEN** the module is loaded, then unloaded, then loaded again
- **THEN** no dangling implementation SHALL remain and creation SHALL succeed again after reload

### Requirement: Null-backend safety

`NavigationSystem` SHALL tolerate the absence of a registered factory and SHALL not crash when no backend is
loaded.

#### Scenario: World attaches without a backend

- **WHEN** a `NavigationSystem` is attached to a world with no registered factory
- **THEN** `GetNaviMesh()` SHALL be null and no crash SHALL occur

### Requirement: Validated runtime settings and lifetime

The runtime SHALL apply reflected settings to meshes. Invalid/mismatched settings SHALL reject creation with a diagnostic. Registration SHALL retain callback code leases and shutdown SHALL remove only session-owned registrations.

#### Scenario: Custom settings

- **WHEN** a world configures custom resolution and agent dimensions
- **THEN** generation SHALL use those values

#### Scenario: Invalid settings

- **WHEN** settings contain invalid or non-finite dimensions
- **THEN** navigation creation SHALL fail explicitly without assertions or exceptions

#### Scenario: Retained objects

- **WHEN** the module shuts down with referenced meshes/tasks
- **THEN** new factory creation SHALL stop and retained objects SHALL keep implementation code alive

### Requirement: Cooked mesh runtime binding

The reflected navigation config SHALL accept a NaviMesh asset reference and choose eager loading or tile streaming. Runtime initialization SHALL verify asset loading and backend installation explicitly. An empty reference SHALL preserve programmatic generation. Offline cooking SHALL NOT load a previous product as its input navigation mesh.

#### Scenario: Eager cooked mesh

- **WHEN** a valid cooked mesh is configured with streaming disabled
- **THEN** runtime initialization SHALL load an independent queryable mesh before world attachment

#### Scenario: Streamed cooked mesh

- **WHEN** a tiled mesh is configured with streaming enabled
- **THEN** runtime initialization SHALL own its manifest and page it around the streaming focus

#### Scenario: Missing configured mesh

- **WHEN** the asset is missing, fails loading or cannot be installed
- **THEN** navigation initialization SHALL fail with a diagnostic

