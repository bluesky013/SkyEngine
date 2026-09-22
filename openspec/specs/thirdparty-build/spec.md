# thirdparty-build Specification

## Purpose
TBD - created by archiving change thirdparty-explicit-output. Update Purpose after archive.
## Requirements
### Requirement: Explicit third-party output directory
The third-party build tool SHALL require an explicit output directory and SHALL NOT silently default to a path
inside the repository (`<engine>/build_3rd`).

#### Scenario: Output directory provided
- **WHEN** the tool is invoked with an output directory
- **THEN** the platform output tree SHALL be written there and the generated cmake cache SHALL point at it

#### Scenario: No implicit default
- **WHEN** no output directory is provided
- **THEN** the tool SHALL fail with a clear message rather than writing into the repository by default

### Requirement: No default archive or archive config writes
The tool SHALL NOT zip the output or write archive (file/MD5) information into a tracked configuration file by
default. `cmake/thirdparty.json` SHALL remain a dependency list without an `archives` section.

#### Scenario: Build does not modify tracked config
- **WHEN** a platform is built
- **THEN** no archive file SHALL be produced and `cmake/thirdparty.json` SHALL NOT be modified

#### Scenario: Optional packaging is explicit
- **WHEN** packaging is desired
- **THEN** it SHALL be an explicit, opt-in step that does not mutate tracked files

### Requirement: CMake consumes configuration, not archives
The CMake build SHALL obtain the third-party location from configuration (`3RD_PATH` / `SKY_THIRD_PARTY_*`) and
SHALL NOT read an `archives` entry.

#### Scenario: Configure with an explicit path
- **WHEN** the project is configured with the third-party path set
- **THEN** it SHALL locate the third-party packages without any `archives` entry in `thirdparty.json`

### Requirement: Generated state is not tracked
Per-platform build metadata and the cmake cache SHALL live under the output directory and SHALL NOT be tracked.

#### Scenario: Metadata stays outside the tree
- **WHEN** a package is built
- **THEN** its build metadata SHALL be written under the output directory and SHALL NOT appear as a tracked
  change

