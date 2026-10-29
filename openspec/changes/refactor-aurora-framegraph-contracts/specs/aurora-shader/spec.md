## ADDED Requirements

### Requirement: Android Vulkan shader baseline matches the supported device API

The existing Android Vulkan target environment SHALL use SPIR-V 1.3 as its Vulkan 1.1 baseline unless a separately declared higher target is requested. TargetEnvironment profile and minimumVersion SHALL identify that baseline consistently through compilation, the complete request/artifact identity, distribution requirements and runtime compatibility checks. The request/artifact identity SHALL include the canonical target environment so earlier higher-version products are recooked rather than mislabeled. Producer identity SHALL independently describe the compiler/build and source-finalization policy; a target-environment change alone SHALL NOT require changing producer configuration. Code generation SHALL actually emit a compatible module; changing metadata without changing emitted SPIR-V SHALL NOT satisfy the contract. Other platform target environments SHALL retain their own declared profiles.

#### Scenario: Cook for the Android Vulkan baseline
- **WHEN** the normal Android shader distribution is cooked for the existing Vulkan renderer
- **THEN** its profile, minimum version and emitted SPIR-V match the 1.3 baseline, and artifact identity/requirements describe the actual code

#### Scenario: Prior higher-version artifact exists
- **WHEN** cached Android products were compiled under the earlier higher-version policy
- **THEN** the changed environment-bearing request/artifact identity prevents silently reusing or relabeling them and the public cook regenerates matching products without requiring a producer-configuration change

#### Scenario: Device cannot consume a requested artifact
- **WHEN** a shader requires a version or capability beyond the selected physical device's enabled support
- **THEN** compatibility validation reports an explicit error without runtime source compilation or lowering the recorded requirement to hide the mismatch
