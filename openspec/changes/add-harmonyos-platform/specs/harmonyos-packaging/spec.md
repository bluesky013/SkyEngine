## ADDED Requirements

### Requirement: Per-platform install identity
Android, HarmonyOS, iOS and macOS exports SHALL obtain their install identity only from the selected export preset's `bundleId`. The project descriptor SHALL NOT store or resolve an application identifier. Android's Java namespace SHALL come from its project-owned launcher package declaration and SHALL remain independent of the install identity.

#### Scenario: Independent platform packages
- **WHEN** Android and HarmonyOS presets specify different bundle IDs
- **THEN** each generated package SHALL use its own preset ID without changing the other platform or rewriting Java source

#### Scenario: Missing identity during migration
- **WHEN** an existing project has only a descriptor applicationId and no valid preset bundleId
- **THEN** export SHALL fail before modifying output and identify the required per-preset migration

#### Scenario: Native host restarts within one process
- **WHEN** a launcher closes one engine session and creates another in the same process
- **THEN** process metadata handlers and code leases SHALL remain registered while drained session assets, indexes and bundle filesystems are released
- **AND** the second session SHALL load and render its cooked scene without repeating metadata registration

### Requirement: Project-owned OHOS application host
The existing project creation workflow SHALL generate a HarmonyOS ArkTS/Native launcher under the project-owned native directory, using a Huawei DevEco HarmonyOS application configuration and Stage/UIAbility host. The launcher SHALL initialize the existing GameApplication/module system and SHALL NOT duplicate engine implementation sources or introduce a separate Aurora plugin. Product SDK, compile/minimum API, ABI and native library identity SHALL be explicit in generated configuration.

#### Scenario: Create a HarmonyOS game
- **WHEN** a project is generated for the HarmonyOS platform
- **THEN** its generated application shell SHALL contain matching XComponent/Native library names, application metadata and CMake/hvigor configuration linked to existing engine targets

#### Scenario: Repeated native attachment
- **WHEN** the application recreates its XComponent without ending the engine session
- **THEN** the host SHALL reattach the native window without duplicating module initialization or process-wide registrations

### Requirement: Shared export preset and host cooking
HarmonyOS packaging SHALL consume the existing export preset model and manifest. Added fields SHALL round-trip through reflection-driven serialization and the packaging CLI. Assets and Vulkan shader products SHALL be cooked by existing host tools and staged with consistent asset indexes and bundle selection; device builds SHALL exclude host-only builders and compilers in the initial runtime profile.

#### Scenario: Preset round-trip
- **WHEN** a HarmonyOS preset is saved by the reflected editor model and read by the packaging CLI
- **THEN** its platform, ABI, application identity and packaging settings SHALL be preserved

#### Scenario: Cooked scene deployment
- **WHEN** the user packages a scene containing mesh, material, texture and shader dependencies
- **THEN** the HAP SHALL contain the selected cooked products and indexes, and the runtime SHALL resolve them through its bundle filesystem without runtime shader compilation

### Requirement: Verifiable HAP and dependency closure
The packaging workflow SHALL produce an identifiable HAP with the required arm64 native dependencies and a consistent C++ runtime. SDK or build-tool failure SHALL not be reported as successful packaging using stale output. Signing configuration SHALL come from external credentials without embedding secrets in templates or logs; unsigned construction and signed device installation SHALL be distinguished.

#### Scenario: Single owner for application resources
- **WHEN** generate/package prepares HarmonyOS resources and invokes Hvigor
- **THEN** configs, cooked products and development shader sources SHALL be complete before Hvigor starts, and Native CMake SHALL NOT rewrite rawfile during compilation

#### Scenario: Missing transitive native dependency
- **WHEN** a launcher or backend dependency is absent from the HAP and is not supplied by the selected OS SDK
- **THEN** package verification SHALL fail with the missing library identified before reporting the artifact ready

#### Scenario: Packaging fails after an earlier successful build
- **WHEN** cook, configure, hvigor or signing fails while an older HAP remains on disk
- **THEN** the workflow SHALL return failure and SHALL NOT present the older HAP as the new build

#### Scenario: Device acceptance
- **WHEN** a compatible signed HAP is installed on the chosen device
- **THEN** validation SHALL record install/startup, visible cooked scene rendering, input, background/resume, surface recreation and clean exit independently of SDK-only results

#### Scenario: Debug signing does not authorize the target device
- **WHEN** signing configuration does not match the application identity or authorize the chosen HarmonyOS device
- **THEN** deployment SHALL report that failure without modifying credentials implicitly or claiming device acceptance

### Requirement: Shared mobile orientation policy
The shared export preset SHALL expose `orientation` through reflection, JSON serialization and the packaging CLI. Omitted values and the explicit `default` alias SHALL select fixed `landscape`. Android, HarmonyOS and iOS generation SHALL support `portrait`, `auto_rotation`, `auto_rotation_landscape` and `auto_rotation_portrait` in addition to `landscape`, mapped to each platform's manifest or plist. Invalid values SHALL fail without replacing valid generated configuration. Desktop presets SHALL accept only the inert landscape/default value.

#### Scenario: Generate default landscape
- **WHEN** a mobile preset omits orientation or specifies `default`
- **THEN** generation SHALL request fixed landscape using the Android activity, HarmonyOS main ability or iOS supported-orientation metadata

#### Scenario: Preserve project-owned metadata
- **WHEN** generation applies an orientation override
- **THEN** it SHALL modify generated copies only, preserve unrelated metadata and other HarmonyOS abilities, and update device-qualified iOS orientation arrays consistently

#### Scenario: Reject a legacy iOS launcher
- **WHEN** the project-owned iOS launcher does not consume the generated plist path
- **THEN** generation SHALL report the required CMake migration instead of silently ignoring the preset
