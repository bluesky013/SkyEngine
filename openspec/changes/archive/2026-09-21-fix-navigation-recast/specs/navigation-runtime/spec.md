## ADDED Requirements

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
