## ADDED Requirements

### Requirement: Engine modules expose interfaces and data only

An `engine/<feature>` module SHALL contain only interface and data definitions (and pure functions over that data); it SHALL NOT contain the feature's implementation.

#### Scenario: Engine module has no implementation

- **WHEN** an engine feature module is inspected
- **THEN** it SHALL expose interfaces, data types, and pure utilities only, with no runtime implementation of the feature

### Requirement: Features are implemented as plugins

A feature SHALL be implemented in a plugin under `plugins/`, which is the module boundary; the plugin provides the concrete implementation of the engine interfaces.

#### Scenario: Plugin implements the engine interface

- **WHEN** a feature plugin is loaded
- **THEN** it SHALL provide the concrete implementation registered with the engine interface

#### Scenario: Engine does not depend on a plugin

- **WHEN** an engine module is compiled and linked
- **THEN** it SHALL NOT link or include a plugin's implementation

### Requirement: Consumers depend on interfaces

Consumers SHALL depend on the engine interfaces and resolve the concrete implementation at runtime (world sub-system by name), never on a plugin's concrete types.

#### Scenario: Consumer resolves by name

- **WHEN** a consumer needs the feature at runtime
- **THEN** it SHALL obtain the implementation through the engine interface (e.g. the world sub-system by name)

#### Scenario: Feature absent is handled

- **WHEN** the feature plugin is not present in a world
- **THEN** the consumer SHALL treat the feature as unavailable rather than fail to build

### Requirement: Terrain implementation lives in a plugin

The terrain feature SHALL keep only interfaces and the data those interfaces reference (`TerrainMeta`, tile coordinates/info/format, addressing, region sink/change listener, `ITerrainField`/`ITerrainSystem`) in `engine/terrain`, and SHALL implement the field, sub-system, procedural generation, source, asset payload/traits + serialization/registration, LOD description, asset builder, collision layer, component, and builder module in `plugins/terrain`.

#### Scenario: Engine terrain module is interface/data only

- **WHEN** `engine/terrain` is compiled
- **THEN** it SHALL expose only terrain interfaces/data and SHALL NOT link the terrain implementation

#### Scenario: Terrain plugin implements the interfaces

- **WHEN** `plugins/terrain` is built
- **THEN** it SHALL implement `ITerrainSystem`/`ITerrainField` and register the terrain component and asset handling

### Requirement: Vegetation implementation lives in a plugin

The vegetation feature SHALL keep only the surface provider/listener seam, the data referenced by the system interface, and `IVegetationSystem` in `engine/vegetation`, and SHALL implement placement, the sub-system, asset payload/traits + serialization/registration, component, terrain surface bridge, render, and editor in `plugins/vegetation`.

#### Scenario: Engine vegetation module is interface/data only

- **WHEN** `engine/vegetation` is compiled
- **THEN** it SHALL expose only vegetation interfaces/data and SHALL NOT link the vegetation implementation

#### Scenario: Vegetation plugin implements the interfaces

- **WHEN** `plugins/vegetation` is built
- **THEN** it SHALL implement the vegetation sub-system and providers and register the vegetation component and asset handling

### Requirement: Stable include paths during the restructure

The restructure SHALL keep the public include paths (`terrain/...`, `vegetation/...`) stable so moving implementation files does not churn consumers' includes.

#### Scenario: Includes unchanged

- **WHEN** implementation files move from `engine/` to `plugins/`
- **THEN** consumer include directives SHALL remain valid
