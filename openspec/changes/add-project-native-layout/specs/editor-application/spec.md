# Delta: editor-application

## MODIFIED Requirements

### Requirement: Editor host target

The editor host SHALL be started by the dedicated `SandboxEditor` executable (`engine/sandbox/app`). There is no
`Launcher --app editor` mode (the engine no longer ships a launcher). The editor host SHALL load its modules from
`configs/modules_editor.json` (falling back to `SandboxModule` when the config is absent). The `SandboxEditor` target
SHALL build with `SKY_BUILD_SANDBOX`; the core tests SHALL build with `SKY_BUILD_TEST` and run headless.

#### Scenario: Editor host builds and runs the module

- **WHEN** the project is configured with `SKY_BUILD_SANDBOX=ON`
- **THEN** the `SandboxEditor` target SHALL build, open a non-Qt native window, and run the editor module

#### Scenario: Headless tests run

- **WHEN** the project is configured with `SKY_BUILD_TEST=ON`
- **THEN** `EditorCoreTest` SHALL build and run without a window or GPU
