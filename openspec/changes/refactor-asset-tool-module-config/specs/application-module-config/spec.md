## ADDED Requirements

### Requirement: Hosts share module manifest parsing

Framework SHALL provide a reusable parser for the existing modules array with name, dependencies and optional buildOption fields. It SHALL use existing ModuleInfo data and the generated build-option lookup, return explicit diagnostics, and validate the entire bounded manifest before the host registers its selection. It SHALL reject malformed fields, duplicate module declarations, unknown build options and conflicting or cyclic dependency selections. Implicit metadata dependency nodes supported by ModuleInfo SHALL remain valid. File lookup and host-specific settings SHALL remain host responsibilities.

#### Scenario: Invalid later entry
- **WHEN** a later module entry is malformed after earlier entries were parsed
- **THEN** parsing SHALL fail without publishing the earlier entries into ModuleManager

#### Scenario: Disabled feature
- **WHEN** an entry names a known build option whose value is false
- **THEN** that entry SHALL be excluded without attempting to load its module
- **AND** an active entry depending on that explicitly excluded module SHALL report a dependency conflict

#### Scenario: Dependency has no explicit top-level entry
- **WHEN** a module declares an existing metadata dependency not listed as a separate top-level module
- **THEN** the parser SHALL preserve that dependency for the existing module resolution path

#### Scenario: Game and editor reuse the parser
- **WHEN** GameApplication or EditorApplication uses the common parser
- **THEN** its established file precedence, project-module selection and host fallback behavior SHALL remain unchanged

### Requirement: AssetTool selects configuration by execution mode

AssetTool SHALL select its modules from a deployed modules_asset_tool.json for cook/worker/migration modes or modules_asset_audit.json for product auditing. These manifests SHALL use the existing schema. Enabled business module identities and dependency lists SHALL NOT be duplicated in AssetTool C++ or dedicated SKY_HAS_*_BUILDER macros. Missing or invalid selected configuration SHALL fail startup explicitly without a hardcoded-list fallback. Configuration lookup SHALL be available before source filesystem initialization.

#### Scenario: New builder is enabled
- **WHEN** a supported builder and its existing target are added to the cook manifest
- **THEN** AssetTool SHALL load it without adding a module-specific branch or name to its C++ startup code

#### Scenario: Selected configuration is missing
- **WHEN** the configured engine/bundle root lacks the manifest selected by the execution mode
- **THEN** startup SHALL report the missing configuration and fail before serving requests

### Requirement: Build and deployment follow the selected manifests

AssetTool build dependencies, module availability collection and configuration deployment SHALL derive from the same manifests and effective feature selections used at runtime. Manifest edits SHALL trigger dependency regeneration. Enabled in-repository module targets and their required closure SHALL be built and deployed when AssetTool is selected; disabled entries SHALL NOT require those targets. Dynamic dependencies SHALL remain build-only rather than becoming host link dependencies.

#### Scenario: Build only AssetTool
- **WHEN** a clean build selects AssetTool with cooking and compression features enabled
- **THEN** the tool bundle SHALL contain its selected module closure and both manifests without requiring an earlier editor build

#### Scenario: Manifest changes
- **WHEN** an enabled module selection changes in a manifest
- **THEN** the next build SHALL refresh the dependent target/deployment selection

### Requirement: Product audit activates only its runtime reading closure

Product audit SHALL read the deployed audit manifest without mounting project source assets or loading project source configuration. It SHALL activate only the product-reading dependency closure, never builder/editor/gameplay runtime modules or on-demand cook. Static factory collection SHALL distinguish available compiled factories from modules selected for activation, so linked cook factories SHALL NOT cause their Init or RegisterMeta to execute during audit.

#### Scenario: Source-free audit
- **WHEN** only the deployed tool bundle and packaged products are available
- **THEN** auditing SHALL verify products without source access or builder registration

#### Scenario: Static tool contains cook factories
- **WHEN** a statically packaged AssetTool runs product audit
- **THEN** factories linked for cook mode SHALL remain inactive unless they are in the audit manifest's dependency closure

#### Scenario: Decoder is unavailable
- **WHEN** a product requires a decoder excluded from the effective audit configuration
- **THEN** audit SHALL fail explicitly without cooking a replacement or accepting unread data

### Requirement: Configuration preserves existing module ownership and failure contracts

Configuration-driven registration SHALL retain ModuleManager's unique metadata owner, code leases and symmetric runtime lifecycle. Missing enabled modules or initialization failures SHALL fail host startup and use existing cleanup; they SHALL NOT be described or treated as successful optional skips. The configuration path SHALL NOT introduce duplicate metadata registration guards, a separate module registry or a new core target.

#### Scenario: Enabled DLL is missing
- **WHEN** an enabled module's DLL cannot be loaded
- **THEN** the host SHALL return startup failure with the module identity and reason
- **AND** initialized resources SHALL follow the existing shutdown path
