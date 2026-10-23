## ADDED Requirements

### Requirement: Optimized Develop configuration with symbols
The engine and generated game CMake entry SHALL support Develop in single- and multi-configuration generators, with optimization and debug symbols. Develop SHALL use the Release CRT/STL ABI and prebuilt Release third-party libraries without defining `_DEBUG`. Debug, Release, RelWithDebInfo and MinSizeRel SHALL remain available. Runtime binaries SHALL use the Develop output subdirectory on desktop.

#### Scenario: Build a Develop editor
- **WHEN** the user builds SandboxEditor with configuration Develop
- **THEN** the host and configured runtime modules are built consistently, link Release third-party libraries and produce optimized binaries with debug symbols in output/bin/Develop

#### Scenario: RelWithDebInfo compatibility
- **WHEN** an existing consumer selects RelWithDebInfo
- **THEN** it retains that configuration and receives compatible Release third-party libraries rather than missing links or Debug CRT libraries

#### Scenario: Open a generated Visual Studio solution with tests
- **WHEN** the engine solution is loaded in Visual Studio with test targets enabled
- **THEN** projects and solution folders have distinct names within each parent, Core and Framework remain loaded with Develop build mappings, and dependent targets build their required libraries

### Requirement: Assertion policy is independent of CRT and NDEBUG
Engine assertions SHALL be enabled by default in Debug and Develop and disabled by default in Release/RelWithDebInfo. Enabled engine assertions SHALL detect failed invariants even when NDEBUG is defined. Assertion expressions SHALL be evaluated once in both enabled and disabled configurations, preserving existing side effects. Assertion failures SHALL identify the expression and source location; recoverable errors SHALL remain explicit results.

#### Scenario: Develop invariant fails
- **WHEN** a Develop engine assertion evaluates false with NDEBUG defined
- **THEN** it reports the invariant and terminates without silently passing

#### Scenario: Disabled assertion expression
- **WHEN** an assertion with a side effect executes in Release
- **THEN** the side effect occurs exactly once and no assertion termination occurs

### Requirement: Diagnostic configuration and development tool discovery
CMake SHALL provide AUTO/ON/OFF policies for engine assertions, GPU validation and resource names. AUTO SHALL enable GPU validation only in Debug and derive resource names from SKY_DEVELOP. Invalid policy values SHALL fail configuration. Existing profiling and explicit tool overrides SHALL remain supported. Root/game presets SHALL expose Develop without replacing publishing presets. Cook tool discovery SHALL prefer existing Develop tools and retain legacy configuration fallbacks.

#### Scenario: Develop GPU validation override
- **WHEN** Develop is configured with SKY_GPU_VALIDATION=ON
- **THEN** the editor and runtime host request GPU validation consistently without changing CRT configuration

#### Scenario: Discover tools with multiple builds present
- **WHEN** both Develop and Debug cook tools exist and no explicit tool is supplied
- **THEN** discovery selects the Develop tool with its sibling modules; if absent it uses a supported fallback

#### Scenario: Vulkan resource naming without validation
- **WHEN** resource names are enabled and GPU validation is disabled
- **THEN** Vulkan requests debug utilities when supported, without enabling validation layers or a debug messenger
