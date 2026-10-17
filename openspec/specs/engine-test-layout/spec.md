# engine-test-layout Specification

## Purpose
TBD - created by archiving change colocate-engine-module-tests. Update Purpose after archive.
## Requirements
### Requirement: Engine module tests are colocated with their module
Test sources, test helpers, and test fixtures for an engine module SHALL live under that module's directory (for example `engine/core/test/` and `engine/framework/test/`) and SHALL be configured by that module's CMake when `SKY_BUILD_TEST` is enabled. Engine tests SHALL NOT depend on a repository-wide `engine/test` aggregator or reach into another module's test directory by source path.

#### Scenario: Core tests are configured by Core
- **WHEN** `SKY_BUILD_TEST` is enabled
- **THEN** `engine/core` SHALL configure `CoreTest` from `engine/core/test`, and no central test directory SHALL be required

#### Scenario: Framework tests are configured by Framework
- **WHEN** `SKY_BUILD_TEST` is enabled
- **THEN** `engine/framework` SHALL configure `FrameworkTest` and its helpers from `engine/framework/test`

#### Scenario: UI tests are configured by UI
- **WHEN** `SKY_BUILD_TEST` is enabled
- **THEN** `engine/ui` SHALL configure its UI test targets from `engine/ui/test`, including the embedded UI shader contract test already stored with the UI render sources

#### Scenario: No cross-module test source paths
- **WHEN** module test CMake is inspected
- **THEN** it SHALL NOT reference sources under another engine module's `test` directory through `${ENGINE_ROOT}/engine/...`

### Requirement: Framework test helpers move with FrameworkTest
The `ProcessHelper` and `CookWorkerStub` executables, the framework fixture assets/configs, and the generated engine/project root header SHALL live in `engine/framework/test` and SHALL be built only with `FrameworkTest`. The generated project root SHALL point at the framework test fixture root.

#### Scenario: Helper executables deploy beside FrameworkTest
- **WHEN** `FrameworkTest` is built
- **THEN** `ProcessHelper` and `CookWorkerStub` SHALL be built and deployed next to it, preserving the process and out-of-process cook tests

#### Scenario: Fixture root survives relocation
- **WHEN** `AssetManagerTest` resolves its project fixture files
- **THEN** the generated `PROJECT_ROOT` SHALL point at the relocated framework test fixtures

### Requirement: Existing test target names remain stable
The colocation move SHALL preserve the existing test executable names: `CoreTest`, `FrameworkTest`, `UICoreTest`, `UITextTest`, `UITextFreeTypeTest`, `UIEditorTest`, `UIShaderTest`, `ProcessHelper`, and `CookWorkerStub`.

#### Scenario: Documented test commands still resolve
- **WHEN** a developer runs a previously documented test target name
- **THEN** the same test executable SHALL exist after the move

### Requirement: Legacy render tests are removed
The colocation change SHALL remove `RenderTest` and `VulkanTest`, which only exercise the deprecated legacy `RenderCore` and `VulkanRHI` stack. The remaining legacy render/shader sources SHALL stay behind their existing opt-in switch and existing retirement tracking.

#### Scenario: Default build has no legacy test targets
- **WHEN** the project is configured with the default `SKY_BUILD_LEGACY_RENDER=OFF`
- **THEN** no `RenderTest` or `VulkanTest` target SHALL be defined

#### Scenario: Legacy stack retirement stays separate
- **WHEN** the legacy render/shader source tree is inspected after this change
- **THEN** it SHALL be unchanged except for removal of its test entries and test source directories

