## ADDED Requirements

### Requirement: Production translation units have one compile owner

The build SHALL assign each production translation unit one primary target. Consumers SHALL reuse that target by linking a static or object library instead of recompiling its sources. The rule SHALL apply to direct target declarations, build wrappers, GLOB results and later target_sources additions. Tests SHALL NOT compile production sources merely because their owner is disabled. Publishing production translation units through interface sources SHALL fail configuration.

#### Scenario: Duplicate hidden in a glob
- **WHEN** a module implementation and another target include the same production source through different paths or GLOB variables
- **THEN** configuration SHALL fail with the source and target identities
- **AND** the diagnostic SHALL direct the consumer to link the implementation target

#### Scenario: Production owner disabled
- **WHEN** a test includes a production source while its normal target is disabled
- **THEN** configuration SHALL still reject that source in the test

#### Scenario: Legitimate object reuse
- **WHEN** multiple consumers link one object/static implementation target or reference its TARGET_OBJECTS
- **THEN** the build SHALL accept the single compilation owner

### Requirement: Exceptions preserve precise test contracts

The build SHALL permit shared test-only fixtures and explicitly declared compilation variants. A production-source variant SHALL identify an existing owner, one exact source, a reason and required distinct compile definitions; the consuming target SHALL be a test. Both targets SHALL actually compile that source, and the declared differences SHALL be present. Generated code SHALL remain subject to unique ownership, while HEADER_FILE_ONLY sources SHALL not count as compiled units.

#### Scenario: Static factory variant
- **WHEN** a test compiles the production module entry with a distinct static factory tag and declares its exact contract
- **THEN** configuration SHALL accept the variant
- **AND** removing the source or required definition SHALL make the declaration fail

#### Scenario: Shared test helper
- **WHEN** multiple test executables or DLL probes compile a helper located in a test-only directory
- **THEN** configuration SHALL accept that fixture without exempting production sources

### Requirement: Ownership validation is automatic and regression tested

The common build entry SHALL automatically validate the complete configured target graph after deferred source additions, including independent game projects. It SHALL normalize source identity and SHALL fail on source expressions it cannot validate. The guard SHALL have executable positive and negative CMake fixtures exposed through CTest and independent CI without engine third-party SDKs. It SHALL NOT provide a global bypass switch.

#### Scenario: Later source addition
- **WHEN** a later target_sources call or deferred call introduces cross-directory production source borrowing
- **THEN** the automatic final pass SHALL reject the configuration

#### Scenario: Game launcher generated registration
- **WHEN** an independent game project attaches explicitly generated registration from its build directory to the launcher
- **THEN** the ownership check SHALL accept that unique generated source
