# python Specification

## Purpose

Define Python embedding, engine bindings, platform packaging and runtime dependencies.

## Requirements

### Requirement: Registered types are exposed to Python

The binding layer SHALL create a Python type for each type registered in the runtime reflection context, resolved
lazily by name, without per-type hand-written binding code.

#### Scenario: Look up a registered type

- **WHEN** a script requests a registered type by name
- **THEN** it SHALL receive a Python type representing that C++ type

#### Scenario: Unknown type

- **WHEN** a script requests a type name that is not registered
- **THEN** the lookup SHALL raise a Python exception naming the missing type

### Requirement: Instances can be constructed

The binding layer SHALL construct reflected instances using the registered constructors, including the default
constructor.

#### Scenario: Default construction

- **WHEN** a script constructs a type that has a default constructor
- **THEN** it SHALL receive an instance wrapping a default-constructed value

#### Scenario: Argument construction

- **WHEN** a script constructs a type with arguments matching a registered constructor
- **THEN** it SHALL receive an instance constructed from those arguments

### Requirement: Member access reflects C++ fields

The binding layer SHALL expose reflected members as readable and, when writable, assignable Python attributes
that read and write the same underlying C++ field.

#### Scenario: Read a member

- **WHEN** a script reads a member of an instance
- **THEN** it SHALL receive the current value of the underlying field

#### Scenario: Write a member

- **WHEN** a script assigns a writable member of an instance
- **THEN** the underlying field SHALL be updated

#### Scenario: Const member is read-only

- **WHEN** a script assigns a member that is registered as const
- **THEN** the assignment SHALL fail and SHALL NOT modify the underlying field

#### Scenario: Unknown member

- **WHEN** a script reads or writes an attribute that is not a reflected member
- **THEN** the access SHALL raise a Python exception

### Requirement: Enums are exposed

The binding layer SHALL expose reflected enumeration types together with their declared names and values.

#### Scenario: Enum values are accessible

- **WHEN** a script accesses a reflected enum's declared value
- **THEN** it SHALL receive the value associated with that name

#### Scenario: Enum member round-trips

- **WHEN** a script reads an enum-typed member and writes it back
- **THEN** the underlying enum field SHALL retain the same value

### Requirement: Sequence containers are exposed and mutable

The binding layer SHALL expose reflected sequence members as Python sequence objects backed by the underlying
container, supporting length, indexed access, and mutation.

#### Scenario: Length and indexing

- **WHEN** a script queries the length of a sequence member or indexes an element
- **THEN** it SHALL observe the underlying container's size and elements

#### Scenario: Append and erase

- **WHEN** a script appends to or erases from a sequence member
- **THEN** the underlying container SHALL reflect the change

### Requirement: Member functions are callable

The binding layer SHALL expose reflected member functions as Python methods that validate arguments before
dispatching.

#### Scenario: Successful call

- **WHEN** a script calls a reflected member function with matching arguments
- **THEN** it SHALL receive the converted return value

#### Scenario: Argument mismatch

- **WHEN** a script calls a reflected member function with the wrong number or types of arguments
- **THEN** the call SHALL raise a Python exception and SHALL NOT dispatch

### Requirement: Values convert between Python and C++

The binding layer SHALL convert values between Python and C++ for scalars, strings, reflected types, and
reflected sequences.

#### Scenario: Scalar and string round-trip

- **WHEN** a script writes a numeric or string value to a member and reads it back
- **THEN** the value SHALL round-trip unchanged

#### Scenario: Nested reflected value

- **WHEN** a script reads a member whose type is itself registered
- **THEN** it SHALL receive a wrapped instance whose members are accessible

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

### Requirement: The Python core is linked statically on every platform

The Python plugin SHALL link the CPython core statically on every supported platform, so that no Python shared
library is required at runtime.

#### Scenario: No Python shared library dependency

- **WHEN** the Python plugin is loaded on any supported platform
- **THEN** it SHALL NOT require a Python shared library (`python3XX.dll` / `libpython3XX.so`/`.dylib`) to be present
  or loaded

#### Scenario: Single interpreter instance

- **WHEN** the Python plugin is built and loaded
- **THEN** exactly one CPython interpreter SHALL exist for the process

### Requirement: The static core and consumers agree on the linkage mode

The static core and every consumer SHALL be built so the C-API is linked, not imported from a shared library.

#### Scenario: Windows consumer uses static linkage

- **WHEN** the Python plugin compiles against the static core headers on Windows
- **THEN** it SHALL define `Py_NO_ENABLE_SHARED` and SHALL NOT refer to a DLL import library

#### Scenario: Unix static archive

- **WHEN** the core is built on Linux/macOS
- **THEN** a static `libpython3.XX.a` SHALL be produced and linked into the plugin

### Requirement: Required extension modules are built in

The extension modules required for scripting SHALL be compiled into the image and available to `import` without
loading external extension files, on every platform.

#### Scenario: Tier 1 modules import

- **WHEN** a script imports any Tier 1 module (`unicodedata`, `_decimal`, `_uuid`, `_zoneinfo`, `_elementtree`,
  `pyexpat`, `_bz2`, `_lzma`, `_ctypes`, `_socket`, `select`, `_overlapped`, `_queue`)
- **THEN** the import SHALL succeed without loading a separate extension file

#### Scenario: Tier 2 modules are optional

- **WHEN** the Tier 2 build option is disabled
- **THEN** Tier 2 modules (`_ssl`, `_hashlib`, `_sqlite3`, `_asyncio`, `_multiprocessing`) SHALL NOT be required to
  import

#### Scenario: Excluded modules are documented

- **WHEN** a script imports an excluded module such as `_tkinter` or `_wmi`
- **THEN** the failure SHALL be expected and documented, not treated as a packaging defect

### Requirement: The standard library remains bundled

The standard library SHALL be shipped as files and resolvable at runtime on every platform.

#### Scenario: Standard library resolves at runtime

- **WHEN** the interpreter starts with the bundled standard library present
- **THEN** it SHALL locate and import standard library modules without a Python shared library

### Requirement: Debug and Release stay consistent

The static core and the plugin SHALL use matching configuration and runtime on each platform.

#### Scenario: Configuration parity

- **WHEN** the plugin is built in a given configuration
- **THEN** it SHALL link a static core built for the same configuration and a compatible runtime
