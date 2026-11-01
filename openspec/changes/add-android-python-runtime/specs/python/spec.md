## ADDED Requirements

### Requirement: A static CPython core is cross-built for Android

The third-party bootstrap SHALL cross-build a static CPython core for Android with the pinned NDK and install it
under `3RD_PATH/Android/cpython` with headers, the static archive, and the standard library.

#### Scenario: Android package is produced

- **WHEN** the cpython package is built for the Android platform
- **THEN** `3RD_PATH/Android/cpython` SHALL contain `include/`, a static `libpython3.XX.a`, and the standard library

#### Scenario: Missing NDK fails clearly

- **WHEN** the required NDK version is not installed
- **THEN** the build SHALL fail with a message naming the expected NDK version

### Requirement: Android links the static core with builtin extensions

The Android build SHALL link the static core and SHALL provide the Tier 1 extension modules as builtins, with no
shared `libpython` or external extension modules at runtime.

#### Scenario: No shared core dependency

- **WHEN** the Python plugin is loaded on Android
- **THEN** it SHALL NOT require a `libpython3.XX.so` to be present or loaded

#### Scenario: Tier 1 import

- **WHEN** a script imports a Tier 1 module on Android
- **THEN** the import SHALL succeed without loading a separate extension file

### Requirement: The standard library is resolvable on Android

The standard library SHALL be packaged so the interpreter can import it at runtime on Android, and the
configuration SHALL add it to the module search path.

#### Scenario: Standard library import

- **WHEN** the interpreter starts on Android with the packaged standard library present
- **THEN** standard library modules SHALL import successfully

### Requirement: The plugin is packaged in the app

`PythonModule` and the standard library SHALL be included in the Android application package, and the module SHALL
be listed for the Android runtime.

#### Scenario: Module loads in the app

- **WHEN** the Android application starts with the Python plugin packaged
- **THEN** the Python module SHALL be loadable and usable
