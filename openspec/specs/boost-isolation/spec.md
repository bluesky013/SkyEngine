# boost-isolation Specification

## Purpose
TBD - created by archiving change remove-boost. Update Purpose after archive.
## Requirements
### Requirement: Core memory resources use the standard library

`engine/core` SHALL use `std::pmr` for its memory-resource aliases and SHALL NOT depend on
`boost::container`.

#### Scenario: Core builds without Boost

- **WHEN** `engine/core` is built
- **THEN** it SHALL NOT include Boost headers or link a Boost library

#### Scenario: PMR aliases resolve to the standard library

- **WHEN** code uses `PmrResource`, `PmrVector`, `PmrList`, `PmrString`, or `PmrHashMap`
- **THEN** they SHALL resolve to the corresponding `std::pmr` types

### Requirement: Framework does not depend on Boost

`engine/framework` SHALL NOT include or link Boost.

#### Scenario: Framework builds without Boost

- **WHEN** `engine/framework` is built
- **THEN** it SHALL NOT include Boost headers or link a Boost library

### Requirement: Boost is removed from the build

Boost SHALL NOT be discovered, linked, or required by the project; the legacy shader/render modules that used it
have been removed.

#### Scenario: No Boost target is discovered

- **WHEN** the project is configured
- **THEN** no Boost target SHALL be created or linked

#### Scenario: No compiled Boost library required

- **WHEN** the third-party bootstrap runs
- **THEN** no Boost package SHALL be required to build the engine

### Requirement: The legacy compiler link on Aurora is removed

`engine/aurora/core` SHALL NOT link the legacy `ShaderCompiler` target.

#### Scenario: Aurora builds without the legacy compiler

- **WHEN** the Aurora core target is built
- **THEN** it SHALL NOT pull in `ShaderCompiler.Static`

