## MODIFIED Requirements

### Requirement: Features are implemented as plugins

A domain feature SHALL by default provide its concrete implementation in a plugin under `plugins/`, which implements its engine contract. Existing engine foundations such as UI and Animation SHALL be allowed to retain concrete implementations in their established engine module through separated targets and SHALL NOT require a feature-plugin entry merely to satisfy this default. This foundation rule SHALL NOT relocate unrelated feature implementations or alter existing domain-specific ownership requirements.

#### Scenario: Plugin implements the engine interface
- **WHEN** a domain feature plugin is loaded
- **THEN** it SHALL provide the concrete implementation registered with the engine interface

#### Scenario: Engine does not depend on a plugin
- **WHEN** an engine module is compiled and linked
- **THEN** it SHALL NOT link or include a plugin's implementation

#### Scenario: Foundation implementation does not require a plugin
- **WHEN** UI foundation implementation targets are configured within `engine/ui`
- **THEN** those targets SHALL NOT require a UI feature-plugin entry, and unrelated feature ownership SHALL remain unchanged


#### Scenario: Animation remains an engine subsystem
- **WHEN** Animation, GraphAssets, World, Builder and Editor targets are configured
- **THEN** their implementations SHALL reside in engine/animation/{core,assets,adaptor,builder,editor}, with algorithms and pure Graph unified in Animation, unchanged animation/... public includes and no animation plugin registration; Animation SHALL NOT depend on assets, integration, builder or editor targets
