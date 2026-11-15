## ADDED Requirements

### Requirement: Aurora persistent and runtime reflection have distinct owners
Aurora.Metadata SHALL register persistent asset/data schemas and component persistent-data mappings in its process definition. Aurora.Render SHALL register concrete runtime component metadata and creators in its own process definition after Aurora.Metadata. Runtime sessions, Cook and Editor SHALL consume these definitions without duplicate registration or idempotent guards.

#### Scenario: Offline World cook without renderer session
- **WHEN** AssetTool loads Content.Builder.Module and the required metadata/cook modules to cook a World with Aurora persistent components
- **THEN** it discovers the schemas and asset references without loading Aurora.Render or creating a Device/window

#### Scenario: Retry and application recreation
- **WHEN** a renderer session fails initialization, retries, shuts down or a manager is recreated
- **THEN** persistent and runtime reflection definitions remain registered once
- **AND** session services still follow their existing Init/Shutdown ownership

#### Scenario: Editor creates Aurora components
- **WHEN** Aurora.Render.Editor initializes
- **THEN** its declared dependencies provide Aurora.Metadata and Aurora.Render definitions and required current-session services
