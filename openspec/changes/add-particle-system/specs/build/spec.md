## ADDED Requirements

### Requirement: Particle engine build option

The build SHALL declare `SKY_BUILD_PARTICLE` as an engine option independent of `plugins/plugins.json`. The `Particle` core target SHALL remain available when the option is OFF; when ON, the engine SHALL configure the assets, adaptor, builder and editor targets according to their own prerequisites.

#### Scenario: Core remains available when disabled

- **WHEN** the build is configured with `SKY_BUILD_PARTICLE=OFF`
- **THEN** the `Particle` core target SHALL still build and its tests SHALL still be available under `SKY_BUILD_TEST`

#### Scenario: Integration targets when enabled

- **WHEN** `SKY_BUILD_PARTICLE=ON`
- **THEN** the engine SHALL configure `Particle.Assets`, `Particle.World`, `Particle.Render`, and SHALL configure the builder only under the documented developer prerequisite and the editor only under the documented editor prerequisite

#### Scenario: Explicit option survives manifest changes

- **WHEN** the build is configured with an explicit `SKY_BUILD_PARTICLE` value and the plugin manifest changes
- **THEN** the explicit value SHALL remain in effect and `ModuleManager::GetBuildOption` SHALL report it consistently

#### Scenario: Single compilation owner

- **WHEN** particle targets, tests and modules are configured
- **THEN** every production source SHALL retain exactly one compilation owner and tests SHALL link the implementation target rather than compiling production sources
