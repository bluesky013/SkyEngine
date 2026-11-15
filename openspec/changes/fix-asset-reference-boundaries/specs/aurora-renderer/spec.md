## ADDED Requirements

### Requirement: Scene materials resolve independently of geometry
The scene resource source SHALL resolve a material and its required resource ownership without a mesh. Runtime material resolution SHALL consume product identities through the configured loader and SHALL NOT resolve a source filename implicitly.

#### Scenario: Navigation material without Plane
- **WHEN** navigation has valid geometry and a valid material while the Engine Plane mesh is unavailable
- **THEN** navigation material preparation and overlay publication succeed without any mesh load

#### Scenario: Material resource dependencies
- **WHEN** an independently resolved material requires textures or uniform resources
- **THEN** its result retains and publishes those dependencies with the same lifetime contract as mesh material resolution
