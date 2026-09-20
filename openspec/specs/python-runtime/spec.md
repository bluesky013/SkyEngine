# python-runtime Specification

## Purpose
TBD - created by archiving change add-python-runtime. Update Purpose after archive.
## Requirements
### Requirement: CPython is provisioned from source by the third-party bootstrap

The bootstrap SHALL build CPython from source and install it under `3RD_PATH/cpython` in the layout used by the
engine, on desktop platforms, without requiring a pre-existing local interpreter.

#### Scenario: Bootstrap installs an importable CPython tree

- **WHEN** the third-party bootstrap runs with `SKY_BUILD_PYTHON`-required cpython enabled
- **THEN** `3RD_PATH/cpython` SHALL contain the headers, import library, shared runtime, and standard library

#### Scenario: Non-CMake package is not configured with CMake

- **WHEN** a package is marked as produced entirely by its custom build step
- **THEN** the bootstrap SHALL run the custom step and SHALL NOT run a CMake configure/build on that package

#### Scenario: Unsupported platform is skipped explicitly

- **WHEN** the cpython package is built for a platform it does not support
- **THEN** the bootstrap SHALL skip it and report the platform limitation

### Requirement: The build fails clearly when CPython is unavailable

When `SKY_BUILD_CPYTHON` is enabled, CMake SHALL verify the CPython package exists and SHALL fail configuration
with an actionable message if it does not, instead of registering an imported target with missing paths.

#### Scenario: Missing package

- **WHEN** `SKY_BUILD_CPYTHON=ON` but `3RD_PATH/cpython` is absent
- **THEN** configuration SHALL fail with a message naming the expected path

#### Scenario: Version is not hardcoded to one install layout

- **WHEN** the CPython package is present for a supported version
- **THEN** the imported target SHALL resolve its include library and runtime from the package layout rather than a
  hardcoded version string

### Requirement: The interpreter home is resolved at runtime

The Python plugin SHALL NOT contain hardcoded absolute interpreter paths; it SHALL resolve the interpreter home
from the build/install location or an environment override.

#### Scenario: No absolute build-machine path

- **WHEN** the Python plugin is built and run on another machine
- **THEN** interpreter initialization SHALL resolve the standard library without a path that only exists on the
  author's machine

#### Scenario: Environment override

- **WHEN** an interpreter-home environment variable is set
- **THEN** initialization SHALL use it in preference to the default resolution

### Requirement: Interpreter lifecycle is guarded

The plugin SHALL initialize the interpreter once, SHALL record the initialized state, and SHALL finalize only when
it actually initialized the interpreter.

#### Scenario: Idempotent init

- **WHEN** initialization is requested more than once
- **THEN** the interpreter SHALL be initialized at most once and later calls SHALL succeed without re-initializing

#### Scenario: Safe shutdown

- **WHEN** shutdown runs after a successful initialization
- **THEN** the interpreter SHALL be finalized exactly once

#### Scenario: Shutdown without init

- **WHEN** shutdown runs without a prior successful initialization
- **THEN** it SHALL be a no-op and SHALL NOT finalize an uninitialized interpreter

### Requirement: Scripts can be executed

The plugin SHALL provide an API to run a Python file and to run a Python string, and SHALL report failures with the
Python error information.

#### Scenario: Run a file

- **WHEN** a valid Python file is run
- **THEN** the call SHALL succeed and the script's effects SHALL be observable

#### Scenario: Run a string

- **WHEN** a Python source string is run
- **THEN** the call SHALL execute the source in the interpreter's main namespace

#### Scenario: Runtime error is reported

- **WHEN** a script raises an exception
- **THEN** the call SHALL fail and SHALL report the exception text instead of silently succeeding

### Requirement: Runtime loading matches compile-time selection

The Python plugin is opt-in, so the default runtime and editor module manifests SHALL NOT reference the Python
module; enabling the plugin for real use SHALL require adding the matching runtime module entry.

#### Scenario: Default manifests omit the optional module

- **WHEN** the default game or editor module manifest is read
- **THEN** it SHALL NOT reference the Python module

#### Scenario: Enabled plugin is loaded

- **WHEN** the Python plugin is built and its module entry is present in the active manifest
- **THEN** the module SHALL be loaded

### Requirement: Interpreter initialization requires the host environment

The plugin SHALL refuse to initialize the interpreter until the host environment is attached, so the plugin shares
the host's reflection context instead of creating a private one.

#### Scenario: Initialization before attach is rejected

- **WHEN** interpreter initialization is requested before the host environment is attached
- **THEN** initialization SHALL fail and report the missing attachment

#### Scenario: Initialization after attach proceeds

- **WHEN** the host environment is attached before initialization
- **THEN** initialization SHALL proceed

