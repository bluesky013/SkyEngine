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

### Requirement: Boost is isolated and header-only

After this change, Boost SHALL be used only by the legacy shader and legacy render modules, and the Boost
third-party target SHALL be a header-only interface (no compiled Boost library).

#### Scenario: Only legacy modules link Boost

- **WHEN** the project is configured
- **THEN** only the legacy shader and legacy render targets SHALL link the Boost target

#### Scenario: No compiled Boost library required

- **WHEN** the third-party bootstrap provides Boost
- **THEN** no `boost::container` (or other compiled Boost) library SHALL be required to build the engine

### Requirement: The legacy compiler link on Aurora is removed

`engine/aurora/core` SHALL NOT link the legacy `ShaderCompiler` target.

#### Scenario: Aurora builds without the legacy compiler

- **WHEN** the Aurora core target is built
- **THEN** it SHALL NOT pull in `ShaderCompiler.Static`

