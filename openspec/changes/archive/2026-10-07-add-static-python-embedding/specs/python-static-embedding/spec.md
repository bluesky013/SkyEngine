## ADDED Requirements

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
