## MODIFIED Requirements

### Requirement: Editor host target

The editor host SHALL be startable either by the dedicated `SandboxEditor` executable (`engine/sandbox/app`) or by
the launcher in editor mode (`--app editor`); both SHALL run the same editor host logic. The editor host SHALL load
its modules from `configs/modules_editor.json` (falling back to `SandboxModule` when the config is absent). The
`SandboxEditor` target and the editor mode SHALL build with `SKY_BUILD_SANDBOX`; the core tests SHALL build with
`SKY_BUILD_TEST` and run headless.

#### Scenario: Editor host builds and runs the module
- **WHEN** the project is configured with `SKY_BUILD_SANDBOX=ON`
- **THEN** the `SandboxEditor` target SHALL build, open a non-Qt native window, and run the editor module

#### Scenario: Launcher editor mode runs the editor
- **WHEN** the launcher is started with `--app editor` and the sandbox editor is built
- **THEN** it SHALL run the editor host with modules from `configs/modules_editor.json`

#### Scenario: Headless tests run
- **WHEN** the project is configured with `SKY_BUILD_TEST=ON`
- **THEN** `EditorCoreTest` SHALL build and run without a window or GPU
