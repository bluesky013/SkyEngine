## ADDED Requirements

### Requirement: Linux build configuration

The build SHALL configure a Linux target: a Linux branch in the platform configuration and in the `Core` and `Framework` CMake projects, with `Core` reusing the existing Unix async backend and `Framework` compiling the shared POSIX process backend plus a Linux platform backend.

#### Scenario: Configure and build the worker subset

- **WHEN** the project is configured and built on Linux
- **THEN** `Core`, `Framework`, and the `AssetTool` worker SHALL compile and link, and the worker SHALL start and complete the protocol handshake

#### Scenario: Non-worker modules skipped

- **WHEN** the project is configured on Linux
- **THEN** modules outside the worker subset (render, aurora, editor, sandbox, and the rest) SHALL NOT be required to build

#### Scenario: Real cook deferred until builders land

- **WHEN** a cook is requested on Linux, where the asset builder modules are not built
- **THEN** the request SHALL fail observably (never a false success or a stranded load); enabling real Linux cook SHALL require the builder modules and their dependencies to build on Linux in a follow-up

### Requirement: Linux platform identity and paths

The Linux platform backend SHALL report `PlatformType::Linux` and SHALL provide internal and bundle paths derived from the running executable, and environment-variable lookup, without creating a window.

#### Scenario: Platform reports Linux

- **WHEN** the platform is initialized on Linux
- **THEN** `GetType()` SHALL return Linux and the internal/bundle paths SHALL be resolvable from the executable location

#### Scenario: Headless initialization

- **WHEN** the platform is initialized on Linux with no display available
- **THEN** it SHALL succeed without creating a window, so the batch/standalone worker can run

### Requirement: Linux priority and scope

Windows and macOS SHALL land before the Linux backend, which SHALL be tracked as lower priority; the Linux deliverable SHALL be the worker building and completing the protocol handshake (real cook deferred until builders build on Linux), and a windowed Linux engine or editor SHALL be out of scope.

#### Scenario: Linux tasks are separable

- **WHEN** the Linux backend tasks are deferred or dropped
- **THEN** the Windows and macOS process/cook functionality SHALL remain complete and unaffected

#### Scenario: Shared POSIX source

- **WHEN** the POSIX process backend is built for Linux
- **THEN** it SHALL be the same source used for macOS, with no Linux-specific fork of the spawn/pipe logic
