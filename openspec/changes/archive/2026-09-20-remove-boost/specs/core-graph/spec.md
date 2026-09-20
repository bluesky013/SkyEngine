## ADDED Requirements

### Requirement: Core provides a self-contained graph utility

`engine/core` SHALL provide a header-only directed-graph utility with vertex creation, edge creation, and
topological sort, without depending on Boost.

#### Scenario: Topological order puts dependencies first

- **WHEN** edges are added as `from -> to` meaning `from` depends on `to`
- **THEN** the topological order SHALL list `to` before `from` for every edge

#### Scenario: Duplicate edges

- **WHEN** the same edge is added more than once
- **THEN** it SHALL be treated as a single edge and SHALL NOT affect the order

#### Scenario: Empty and single-vertex graphs

- **WHEN** the graph has no edges
- **THEN** the topological order SHALL contain every vertex exactly once

### Requirement: ModuleManager orders modules with the Core graph

`ModuleManager` SHALL use the Core graph utility instead of Boost, preserving load and unload ordering.

#### Scenario: Load order

- **WHEN** modules are registered with dependencies
- **THEN** modules SHALL be loaded with dependencies before dependents

#### Scenario: Unload order

- **WHEN** modules are unloaded
- **THEN** dependents SHALL be shut down before their dependencies (reverse of the load order)

#### Scenario: No Boost in ModuleManager

- **WHEN** the framework module is built
- **THEN** `ModuleManager` SHALL NOT include or link Boost
