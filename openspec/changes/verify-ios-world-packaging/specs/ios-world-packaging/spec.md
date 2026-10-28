## ADDED Requirements

### Requirement: World assets use checked binary products

The existing asset pipeline SHALL cook `.world` authoring JSON into versioned typed binary World products. Actor identity, hierarchy, component persistence Data, typed subsystem configs and asset dependencies SHALL survive roundtrip. Runtime SHALL load World through the existing product index with ProductsOnly. The decoder SHALL reject unsupported schemas, truncation, invalid counts/lengths, duplicate identities and invalid hierarchy before publishing a partial World.

#### Scenario: Cook and run the default World
- **WHEN** a project selects a valid World with Cube, Plane, camera and lights as defaultScene
- **THEN** the public package flow produces a binary World asset, audits its dependency closure and automatically starts that World using the ordinary game template
- **AND** no authoring World JSON is shipped

#### Scenario: Preserve logical symlink identity
- **WHEN** a selected source path aliases another file inside project assets
- **THEN** its logical source/product path remains the selected path
- **AND** a symlink escaping project assets fails validation

#### Scenario: Android collects checked flat products
- **WHEN** an Android APK collects cooked asset products
- **THEN** the selected product index and its referenced UUID-addressed checked product files are packaged together using default asset filters
- **AND** the default World loads from binary products without source access

### Requirement: SPIR-V products omit optional source declarations

The compiler SHALL omit `OpSource` and `OpSourceContinued` from finalized SPIR-V artifacts, retaining all other instructions, native entry identity and reflection. The policy SHALL participate in the SPIR-V producer identity. Malformed instruction boundaries or entry declarations SHALL fail without publishing partial code.

#### Scenario: Older Vulkan source metadata parser
- **WHEN** Slang emits a source-language declaration that an older software Vulkan parser does not recognize
- **THEN** the cooked module contains no optional source declarations and its executable shader semantics remain unchanged

### Requirement: World simulation is independent of presentation

The application SHALL tick the active World before scene extraction on active application frames even when renderer submission is BUSY. The launcher host SHALL own packet sequence/submission, scene/view/output identities and main view revisions.

#### Scenario: Renderer backpressure
- **WHEN** a pending render packet remains BUSY
- **THEN** application simulation continues while the pending render snapshot stays immutable

### Requirement: Output transforms and completed prepare ownership are single-use

Runtime SHALL preserve logical camera projection and let frame composition apply the output transform once. Task drain callback transfer SHALL empty the original owner while retaining callback code/state through the invocation. Completed prepare products SHALL be released after their consumers and active-group ownership retire.

#### Scenario: Rotated Vulkan output
- **WHEN** an Android surface reports a quarter-turn output transform
- **THEN** the logical camera is preserved and the output mapping is applied once without geometry distortion

#### Scenario: Small drain callback retains a job
- **WHEN** a completed prepare job uses a small callable with shared state under libc++
- **THEN** draining clears the original callback and retires the previous products without a self-reference cycle

### Requirement: Launcher scene sessions have explicit ownership and events

A normally initialized launcher SHALL expose one session registration using a real module lease and exact host/generation token; provider-only mode SHALL NOT. Sessions SHALL receive Append context and relevant RendererEvents from the unique poll owner. Replacement SHALL wait for retirement of the previous scene generation. Failed or stale unregister operations SHALL NOT remove another session.

#### Scenario: Dynamic entity churn
- **WHEN** a session repeatedly creates and removes entities
- **THEN** exact EntityDestroyed events reach its Bridge and retired slots are reused

#### Scenario: Session replacement
- **WHEN** an executing session unregisters or is replaced
- **THEN** its callback and code lease survive the current invocation, and the replacement uses a new retired-safe scene generation

### Requirement: Native iOS launch and reproducible fixture

The iOS launcher SHALL include a launch-screen declaration. The reusable scene fixture SHALL use the ordinary project template and parameterized simulator SDK/architecture/dependency paths.

#### Scenario: Modern iPhone canvas
- **WHEN** the configured app runs on iPhone 16 Pro / iOS 18.6
- **THEN** portrait view/framebuffer sizes are 402x874 points / 1206x2622 pixels
