## ADDED Requirements

### Requirement: Render-agnostic physics debug geometry

Physics debug output SHALL be expressed as plain geometry data (positions, colors, and line/triangle primitives) and SHALL NOT reference render resource types.

#### Scenario: Produce debug geometry

- **WHEN** a backend is asked for debug geometry
- **THEN** it SHALL return plain geometry data without any render type

#### Scenario: No render types in the interface

- **WHEN** the physics debug interface is inspected
- **THEN** it SHALL NOT include render headers or accept render resource handles

### Requirement: Physics core has no render dependency

The physics core module SHALL NOT include render headers or link render libraries.

#### Scenario: Core builds without render

- **WHEN** the physics core module is compiled and linked
- **THEN** it SHALL have no render include or render library dependency

#### Scenario: Backend debug output is render-free

- **WHEN** a physics backend supplies debug geometry
- **THEN** it SHALL be able to do so without linking a render adaptor
