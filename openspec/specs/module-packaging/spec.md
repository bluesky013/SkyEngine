# module-packaging Specification

## Purpose
TBD - created by archiving change add-static-module-bundling. Update Purpose after archive.
## Requirements
### Requirement: Module packaging mode selection
The engine SHALL support two runtime module packaging modes - **dynamic** (one shared library per module) and
**static bundling** (selected modules linked into the host/bundle) - and SHALL default to **dynamic** so existing
projects are unchanged.

#### Scenario: Default is dynamic
- **WHEN** a project does not specify a packaging mode
- **THEN** every runtime module is packaged as a dynamic (shared) library

#### Scenario: Static mode selected
- **WHEN** the packaging mode is set to static
- **THEN** each module's static core is linked into the host/bundle and no separate shared library is emitted for it

### Requirement: Static bundling rules
Static bundling SHALL be driven by an explicit rule set (which modules are bundled, and any grouping) rather than by
editing individual modules, and SHALL allow a mixed mode where only the listed modules are static.

#### Scenario: Only listed modules are bundled
- **WHEN** the mode is mixed and a rule set lists specific modules
- **THEN** exactly those modules are statically bundled and all others remain dynamic

#### Scenario: Editor-only modules excluded
- **WHEN** a runtime bundle is produced
- **THEN** modules marked editor-only are not pulled into the static bundle

### Requirement: Dynamic registration behavior is preserved
A statically bundled module SHALL still register with `ModuleManager` at startup; static bundling removes the shared
library, not the registration.

#### Scenario: Statically bundled module registers without a library
- **WHEN** a module is statically bundled
- **THEN** it is registered at startup through a compiled-in registration and no dynamic library is required

#### Scenario: No double registration
- **WHEN** a module is statically bundled and a shared library of the same module is also present
- **THEN** the runtime registers the module exactly once, with the static instance taking precedence

### Requirement: Packaging options are preset-driven
The packaging mode and static bundling rules SHALL be controllable through build/packaging options and SHALL be
exposed through the project export preset, consistent with the project packaging option surface.

#### Scenario: Preset selects the mode and rules
- **WHEN** a project export preset sets the packaging mode and the static module rules
- **THEN** the generated/build project honors them during configure and packaging

