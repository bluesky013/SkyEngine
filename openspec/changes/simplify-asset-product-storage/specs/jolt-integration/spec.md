## MODIFIED Requirements

### Requirement: Pinned and ABI-consistent Jolt dependency

The third-party manifest SHALL pin Jolt v5.6.0 to revision `e77f175595e64cb44218cc9d9d56fc365ad0e36a`, use the existing bootstrap and publish matching Debug/Release artifacts. Import definitions SHALL match the SDK's ABI-affecting build options. The delivered configuration SHALL use float precision, disable C++ exceptions, and preserve the SDK license notice. RTTI SHALL be configured for the target platform and match the consuming C++ ABI rather than being unconditionally disabled; the macOS and Android Clang SDKs and consumers SHALL enable compatible RTTI, while an existing platform override SHALL remain consistent with its consumers.

#### Scenario: Rebuild from an existing dependency checkout
- **WHEN** the Jolt dependency is rebuilt from a stale checkout
- **THEN** the existing version-pinning mechanism SHALL select the pinned revision before configuration and compilation

#### Scenario: Consumer links the dependency
- **WHEN** the backend is compiled against an installed Debug or Release SDK
- **THEN** its CRT, assertion, profiling, debug-renderer, ObjectLayer and SIMD settings SHALL match that SDK

#### Scenario: Platform consumer requires RTTI
- **WHEN** a supported platform's C++ consumers require SDK RTTI symbols, including macOS Clang
- **THEN** the SDK and consumers SHALL use matching RTTI settings while C++ exceptions remain disabled
- **AND** changing these settings SHALL require rebuilding the affected SDK artifacts and consumers

#### Scenario: Android NDK consumer links the arm64 SDK
- **WHEN** Android NDK Clang consumers link the Debug or Release arm64-v8a Jolt SDK
- **THEN** the Android manifest override SHALL enable SDK RTTI to match those consumers while keeping C++ exceptions disabled
- **AND** both configurations SHALL be rebuilt and checked for the selected ABI and emitted RTTI symbols before package/runtime validation

### Requirement: SDK state and executable code outlive physics objects

The backend and every live world SHALL hold an SDK state lease and the necessary module code lease. Unregistering a backend SHALL prevent new world creation but SHALL NOT destroy SDK state or unload code still needed by existing worlds. The last user SHALL release jobs, native objects and callbacks before releasing SDK state and code. The SDK initialization guard SHALL belong to the actual native static Jolt image whose globals it initializes; independently linked native images SHALL NOT share a guard that skips their initialization. Native SDK symbols and private backend implementation types, including their template instantiations, SHALL remain bound to their owning native image so cross-image weak-symbol resolution cannot select another image's allocator or SDK state. Public Physics interfaces and process-shared registries SHALL remain visible; metadata SHALL retain its unique definition owner. This integration SHALL NOT add a module, target or public SDK lifecycle API.

#### Scenario: Registry is unregistered before a world is released
- **WHEN** a backend is unregistered while a created world is still owned by a consumer
- **THEN** that world SHALL retain the state and code needed for safe access and destruction, and a new registry creation request SHALL fail

#### Scenario: World is torn down during shutdown
- **WHEN** the last world is destroyed
- **THEN** all jobs SHALL be joined and constraints, characters, bodies, shapes and listeners SHALL be released before SDK teardown

#### Scenario: Independently linked native images use Jolt
- **WHEN** an executable and a module contain separate static Jolt images
- **THEN** each native image SHALL initialize and retain its own SDK globals for its live users
- **AND** the public physics registry SHALL remain process-shared and component/config metadata SHALL still have one definition owner

#### Scenario: Native and private implementation symbols could be coalesced
- **WHEN** the platform linker can coalesce weak SDK header or private backend template symbols across an executable and module
- **THEN** symbol visibility SHALL keep those implementations, allocations and SDK state within their owning native image
- **AND** the public Physics interface and process-shared registry SHALL remain available across module boundaries

## ADDED Requirements

### Requirement: Authoring builders follow the existing developer build boundary

The Jolt builder directory SHALL be enabled only when `SKY_DEVELOP` or `SKY_BUILD_TEST` is enabled. Existing tool and sandbox options SHALL continue to enable developer facilities through their shared option contract. Shipping without those facilities SHALL retain the runtime, render bridge and shared asset codec without building, statically registering or linking Jolt's authoring builder. An enabled builder SHALL use the existing tagged module entry so its static factory matches CMake-generated registration while preserving the dynamic entry contract.

#### Scenario: Shipping static launcher
- **WHEN** a static launcher is configured with developer facilities, tools, sandbox and tests disabled
- **THEN** Jolt Builder targets SHALL be absent and its factory SHALL appear in neither generated registration nor launcher link inputs
- **AND** the Jolt runtime and render modules SHALL remain available

#### Scenario: Developer or test host requires the builder
- **WHEN** Jolt is configured for developer facilities, a tool, sandbox or tests
- **THEN** its builder targets SHALL remain available to those consumers
- **AND** static registration SHALL request the factory emitted by the builder's tagged entry rather than the dynamic-only entry
