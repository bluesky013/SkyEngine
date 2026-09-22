## REMOVED Requirements

### Requirement: RHI-free shell prototype target

## ADDED Requirements

### Requirement: Editor host target
The editor host (`SandboxEditor`, `engine/sandbox/app`) SHALL build with `SKY_BUILD_SANDBOX` and SHALL load the
editor module (`SandboxModule`) through the engine module system. The core tests SHALL build with `SKY_BUILD_TEST`
and run headless. (The earlier RHI-free `Sandbox` shell has been removed in favour of this single host.)

#### Scenario: Editor host builds and runs the module
- **WHEN** the project is configured with `SKY_BUILD_SANDBOX=ON`
- **THEN** the `SandboxEditor` target SHALL build, open a non-Qt native window, and run the editor module

#### Scenario: Headless tests run
- **WHEN** the project is configured with `SKY_BUILD_TEST=ON`
- **THEN** `EditorCoreTest` SHALL build and run without a window or GPU
