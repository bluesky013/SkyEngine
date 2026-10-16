# editor-build Specification

## Purpose
Keep editor runtime modules coordinated with host-only builds in the selected configuration, preventing stale dynamically loaded binaries.

## Requirements
### Requirement: Editor host build schedules available configured runtime modules
When SandboxEditor is configured and configs/modules_editor.json is available, the top-level build SHALL add a build-only dependency from SandboxEditor to each module name in that manifest that has an existing configured CMake target. Dependency binding SHALL occur after engine/plugin targets are defined. Building SandboxEditor SHALL schedule those dependencies through the same selected build configuration without adding module link dependencies or changing runtime loading behavior.

#### Scenario: Host-only Debug build includes configured plugins
- **WHEN** an enabled manifest module such as BulletPhysicsModule or RecastNavigation has a configured target and SandboxEditor is built in Debug
- **THEN** the generated build graph SHALL schedule that target in the selected configuration before completing the host build

#### Scenario: Optional target is unavailable
- **WHEN** the manifest names a module without a configured target
- **THEN** dependency binding SHALL skip that name without force-enabling a plugin or claiming the module is available at runtime

#### Scenario: Editor host is not configured
- **WHEN** the current configuration has no SandboxEditor target
- **THEN** runtime-module dependency binding SHALL NOT require that target or prevent the configuration from succeeding

### Requirement: Manifest edits refresh the editor dependency selection
The checked-in module manifest SHALL remain the source of the editor's runtime module dependency selection when SandboxEditor is configured. CMake SHALL track it as a configure input so a later change refreshes the generated dependency set on the next build.

#### Scenario: Configured module set changes
- **WHEN** modules_editor.json changes after configuration
- **THEN** the next build SHALL trigger configuration as needed and use the updated set of existing module targets

### Requirement: Startup acceptance identifies the actual host and module configuration
Verification of this ABI-mismatch repair SHALL record a host and configured runtime modules built from the same Debug tree/configuration, together with actual startup exit/crash/log evidence. A completed build SHALL NOT itself be reported as runtime or GPU/display acceptance. The build guarantee SHALL NOT extend to arbitrary external modules or binaries replaced after the build.

#### Scenario: Former stale Bullet module is rebuilt
- **WHEN** the Debug startup is rerun after the host and affected configured modules are rebuilt
- **THEN** the verification record SHALL identify the relevant binary/path evidence and whether the former PostTick dispatch into RTTI recurs

#### Scenario: Startup reaches a separate platform limitation
- **WHEN** module loading is consistent but GPU/display creation cannot complete
- **THEN** verification SHALL distinguish that limitation from the repaired module ABI fault and retain the corresponding platform gate
