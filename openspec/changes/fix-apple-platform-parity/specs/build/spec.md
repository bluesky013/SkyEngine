## ADDED Requirements

### Requirement: Apple dependency identity binds target architecture and SDK

Apple third-party builds SHALL bind the selected platform to an explicit architecture and device/simulator SDK. Cache reuse and successful metadata publication SHALL require matching identity and real native artifacts. Unsupported cross-host prerequisites SHALL fail explicitly without replacing valid prior output metadata.

#### Scenario: Build another macOS architecture on the current host
- **WHEN** MacOS-arm or MacOS-x86 is selected
- **THEN** native/custom builders SHALL use arm64 or x86_64 respectively, and artifacts with another architecture SHALL be rejected

#### Scenario: Change the IOS simulator target
- **WHEN** the IOS device/simulator SDK or architecture selection changes
- **THEN** the package build identity SHALL change and device output SHALL NOT be reused as simulator output

### Requirement: Third-party tools validate their actual inputs

Read-only preflight SHALL verify the toolchain/source inputs supplied by tool packages rather than require a library install directory that those packages do not produce. A missing or stale actual tool input SHALL remain an explicit failure.

#### Scenario: Validate ios-cmake
- **WHEN** ios-cmake has current metadata and its required checked-out toolchain exists
- **THEN** strict preflight SHALL accept that tool without requiring an empty IOS/ios-cmake directory

### Requirement: UIKit regressions build production targets with pinned dependencies

The UIKit native regression fixture SHALL build Core and Framework through their production targets, retain built-in compression, and consume SDK/architecture-compatible SFMT, RapidJSON, LZ4 and pinned GoogleTest sources. GoogleTest SHALL initialize its version through its own top-level project. Test sources SHALL NOT duplicate production translation units.

#### Scenario: Configure the native fixture
- **WHEN** valid simulator dependencies and the pinned GoogleTest checkout are supplied
- **THEN** CMake configuration and linking SHALL succeed without undefined third-party finder commands or manually invented GoogleTest versions
