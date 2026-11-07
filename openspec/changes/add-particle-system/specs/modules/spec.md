## ADDED Requirements

### Requirement: Particle remains an engine subsystem

Particle SHALL be an explicitly authorized engine subsystem exception. Its interfaces, algorithms, assets, World integration, render adaptor, builder and editor SHALL reside under `engine/particle/{core,assets,adaptor,builder,editor}` and SHALL NOT require a `plugins/particle` feature-plugin entry.

#### Scenario: Particle targets are configured

- **WHEN** Particle, Assets, World, Render, Builder and Editor targets are configured
- **THEN** their implementations SHALL reside under `engine/particle` with the documented target split and unchanged `particle/...` public includes

#### Scenario: No particle plugin entry

- **WHEN** the plugin manifest and module host are inspected
- **THEN** Particle SHALL NOT require a feature-plugin registration, and this exception SHALL NOT change ownership rules for unrelated features

#### Scenario: Core dependency boundary

- **WHEN** the Particle core target is inspected
- **THEN** it SHALL depend only on Core and SHALL NOT include or link Framework, Aurora, editor or builder targets

#### Scenario: Adaptor, builder and editor direction

- **WHEN** adaptor, builder and editor targets are inspected
- **THEN** they SHALL depend toward the core/assets targets, and runtime/core targets SHALL NOT depend on editor or builder targets
