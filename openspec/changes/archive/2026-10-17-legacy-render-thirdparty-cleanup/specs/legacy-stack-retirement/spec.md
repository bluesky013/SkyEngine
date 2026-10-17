## ADDED Requirements

### Requirement: Legacy source trees are removed
The repository SHALL NOT contain the pre-Aurora renderer (`engine/render`), the pre-Aurora shader compiler (`engine/shader`), or the legacy Qt editor (`engine/editor`). Aurora SHALL remain the renderer and shader implementation, and the non-Qt sandbox SHALL remain the editor implementation.

#### Scenario: Legacy engine trees are absent
- **WHEN** the repository tree is inspected
- **THEN** `engine/render`, `engine/shader`, and `engine/editor` SHALL NOT exist

#### Scenario: Aurora and sandbox remain
- **WHEN** the default desktop build is configured
- **THEN** `engine/aurora` and `engine/sandbox` SHALL still be configured and their targets SHALL remain available

### Requirement: Legacy build options are removed
The build SHALL NOT expose `SKY_BUILD_LEGACY_RENDER` or `SKY_BUILD_EDITOR`. The sandbox editor SHALL be selected only through `SKY_BUILD_SANDBOX`, and no CMake path SHALL call `find_package(Qt5)`.

#### Scenario: Legacy options are rejected
- **WHEN** a user configures with `-DSKY_BUILD_LEGACY_RENDER=ON` or `-DSKY_BUILD_EDITOR=ON`
- **THEN** the option SHALL have no legacy target or build branch behind it

#### Scenario: Qt5 is not probed
- **WHEN** the project is configured without `SKY_BUILD_EDITOR`
- **THEN** CMake SHALL NOT search for Qt5 or define Qt editor targets

#### Scenario: Sandbox editor selection is unchanged
- **WHEN** `SKY_BUILD_SANDBOX=ON` is configured
- **THEN** the sandbox editor targets SHALL be configured as before

### Requirement: Legacy-only consumers are removed
The XR plugin, the PVS legacy-render glue, and the legacy-only developer tools SHALL be removed. The PVS core visibility logic SHALL remain available without any render dependency.

#### Scenario: XR plugin is gone
- **WHEN** plugins are configured
- **THEN** `plugins/xr` and `XRModule` SHALL NOT exist, and no target SHALL link the legacy `RHI`/`RenderCore`

#### Scenario: PVS core survives
- **WHEN** the PVS plugin is configured
- **THEN** `PVS.Core` and `PVSTest` SHALL build without `RenderCore`, a render module, or a visualizer

#### Scenario: Legacy tools are gone
- **WHEN** the tools directory is inspected
- **THEN** `tools/noise` and `tools/perf` SHALL NOT exist, and the remaining tool targets SHALL build

### Requirement: Legacy-only third-party dependencies are removed
CMake SHALL NOT discover or link `boost`, `taskflow`, `glslang`, `SPIRV-Cross`, `GKlib`, `metis`, `OpenXR`, `gles`, or `implot`. Their find modules and any corresponding package declarations SHALL be removed. Unused `ktx`, `mimalloc`, and `cxxopts` find modules SHALL also be removed.

#### Scenario: No legacy dependency is discovered
- **WHEN** a default build is configured
- **THEN** no removed dependency is searched for or referenced by a target

#### Scenario: Package manifest is clean
- **WHEN** `cmake/thirdparty.json` is inspected
- **THEN** it SHALL NOT declare `GKlib` or `metis`

#### Scenario: Necessary dependencies remain
- **WHEN** the Aurora and sandbox targets are built
- **THEN** their required dependencies (including `slang`, `dxcompiler`, `assimp`, `meshoptimizer`, `stb`, `nanosvg`, `freetype`, and image/compression libraries) SHALL remain available

### Requirement: Current documentation reflects the removal
Current build, plugin, and architecture documentation SHALL describe Aurora and the sandbox editor only, with no legacy render/shader/Qt-editor instructions. Archived OpenSpec changes SHALL remain historical records.

#### Scenario: Current docs have no legacy instructions
- **WHEN** current README, docs, and skill references are inspected
- **THEN** they SHALL NOT instruct users to enable `SKY_BUILD_LEGACY_RENDER` or `SKY_BUILD_EDITOR`

#### Scenario: Historical records are preserved
- **WHEN** `openspec/changes/archive/` is inspected
- **THEN** archived artifacts SHALL remain unchanged
