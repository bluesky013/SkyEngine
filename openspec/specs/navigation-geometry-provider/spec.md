# navigation-geometry-provider Specification

## Purpose
TBD - created by archiving change terrain-navigation-integration. Update Purpose after archive.
## Requirements
### Requirement: Geometry provider seam

The navigation core SHALL define a geometry-provider interface that supplies world-space triangles for a requested world-space bounds through a geometry sink, without referencing terrain, render, or physics types.

#### Scenario: Provider contributes geometry

- **WHEN** a registered geometry provider is asked to collect geometry for a bounds
- **THEN** it SHALL emit world-space triangles to the geometry sink

#### Scenario: No terrain or physics types in the core

- **WHEN** the navigation core module is compiled
- **THEN** its provider interface SHALL NOT include terrain, render, or physics headers

### Requirement: Backend consumes providers

The nav mesh backend SHALL gather geometry from all registered providers in addition to its existing collision-component source.

#### Scenario: Providers feed the mesh build

- **WHEN** a nav mesh build runs with registered providers
- **THEN** the backend SHALL include the providers' triangles in the rasterized geometry

#### Scenario: No providers registered

- **WHEN** a nav mesh build runs with no registered providers
- **THEN** the backend SHALL behave as before, using only the collision-component geometry

### Requirement: Provider registration ownership

The navigation system SHALL hold an opaque list of geometry providers registered by external modules, so that neither the navigation core nor the nav mesh backend depends on the provider implementations.

#### Scenario: External module registers a provider

- **WHEN** an external module registers a geometry provider with the navigation system
- **THEN** the backend SHALL consume it without linking the provider's module

### Requirement: Collision geometry source preserved

The existing collision-component triangle-mesh source SHALL remain available and unchanged alongside the provider seam.

#### Scenario: Collision components still contribute

- **WHEN** a world contains actors with triangle-mesh collision components
- **THEN** their geometry SHALL still be gathered into the nav mesh build

