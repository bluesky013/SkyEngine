## ADDED Requirements

### Requirement: Project scene input uses the launcher renderer owner

The launcher renderer SHALL consume a registered runtime scene source through an Aurora adaptor interface. The project SHALL provide owned scene/resource data and SHALL NOT submit frames or poll renderer events independently.

#### Scenario: Backpressure
- **WHEN** a packet is busy
- **THEN** the host retries the exact pending packet without appending project commands again.

#### Scenario: Source failure
- **WHEN** a source returns an explicit error
- **THEN** the host abandons that unsubmitted packet and follows its renderer failure and shutdown path.

#### Scenario: Source lifetime
- **WHEN** a project module shuts down
- **THEN** it unregisters its source before destroying project state, and previously queued packets retain no source callbacks or borrowed project storage.

### Requirement: Project arena is loaded through World

The project SHALL load the defaultScene World asset before creating dynamic characters. Static geometry, lights and Physics configuration SHALL come from that asset. Characters SHALL use the World's Physics subsystem, and World::Tick SHALL own its stepping. Static and dynamic visual actors SHALL render through RenderSceneBridge.

#### Scenario: Authored scene changes
- **WHEN** an arena transform, gravity or fixed timestep changes in the World asset
- **THEN** the next launch uses the authored configuration for physics and rendering without editing procedural arena arrays.

#### Scenario: Missing scene
- **WHEN** the descriptor's defaultScene cannot be loaded or built
- **THEN** the project reports an explicit error and exits without substituting a procedural arena.

### Requirement: Original character geometry is imported offline

The project SHALL decode the supported NeoX character mesh offline and retain the original positions, topology, normals and UV data in standard OBJ plus source records. Runtime SHALL load the converted geometry, create dynamic World actors and apply the existing visual cage without coupling the renderer to NeoX parsing or cook dependencies. Prototype materials and unimplemented skeletal animation SHALL be identified explicitly.

#### Scenario: Valid original geometry
- **WHEN** the verified danzai01_lod source is converted
- **THEN** its 4,723 vertices and 8,002 triangles match source GIM bounds within quantization tolerance, all indices are valid and the complete payload is consumed.

#### Scenario: Unsupported or corrupt model
- **WHEN** the converter encounters an unsupported layout or corrupt counts, indices or skin weights
- **THEN** it reports a failure instead of exporting guessed geometry.
- **WHEN** the runtime model is missing or has invalid geometry indices
- **THEN** startup fails explicitly instead of displaying a replacement sphere.

### Requirement: Collision response distinguishes impact and control state

The project SHALL track resolved motion separately from drive input, derive punch strength from the post-physics velocity difference, and distinguish light contact from control loss using recovered thresholds. Strong impacts SHALL suppress movement and jumping until the documented prototype recovery policy completes. Recovery SHALL preserve current velocity without injecting another impulse. Unrecovered native extra-body and recovery semantics SHALL remain explicitly identified as approximations.

#### Scenario: Light contact with held input
- **WHEN** a normal grounded impact has strength 7.7
- **THEN** the character stays controlled, separates immediately, and can steer away or tangentially while contact constrains inward acceleration.
- **AND** ending contact releases this constraint without a fixed impact-protection timer.

#### Scenario: Rolling strong impact
- **WHEN** a rolling impact exceeds the 10.5 stronger-hit threshold
- **THEN** drive input is cleared, the resolved outward velocity survives held inward input, and the HUD shows loss and recovery of control.

#### Scenario: Recovery continuity
- **WHEN** recovery completes or a new impact interrupts recovery
- **THEN** motion remains continuous and the previous-velocity snapshot does not produce a false additional hit.

#### Scenario: Replay and single step
- **WHEN** the user selects replay 1, 2 or 4
- **THEN** the project resets into the player-pair, wall/slide or landing showcase respectively, without exposing rolling through these demonstration controls.
- **WHEN** the paused simulation advances one step
- **THEN** its simulation tick and state timers advance together.

### Requirement: Close-up collision showcase

The project SHALL provide player-pair, wall/slide and landing demonstrations through keys 1, 2 and 4, using authored static World actors and dynamic characters. The pair SHALL face each other and the camera SHALL frame collision detail. Rolling is excluded from these demonstration controls.

#### Scenario: Independent tuning and observable response
- **WHEN** a user edits the reflected lab panel
- **THEN** the runtime SHALL use the same bound settings instance for cage integration, contact restitution, camera and playback
- **AND** gravity/drop height SHALL apply on replay, restitution on the next contact, and other parameters on subsequent simulation/render updates
- **AND** the panel SHALL support undo/redo through CommandService
- **AND** the HUD SHALL display the last pair impact's restitution and actual outgoing speed.

#### Scenario: Soft presentation with limited body separation
- **WHEN** the default automatic pair demonstration makes first contact
- **THEN** its approach drive SHALL stop, the body response SHALL use .08 restitution independently of softer cage parameters, and configurable demonstration braking SHALL control the coast
- **AND** landing SHALL affect cage Y velocity without adding an upward body bounce.

#### Scenario: Directional shadow detail
- **WHEN** the active directional light casts shadows
- **THEN** the existing shadow pass SHALL allocate and rasterize a 2048x2048 D32 map with its current PCF/depth comparison behavior.


#### Scenario: Bounded wall compression
- **WHEN** the extra probe reaches a static vertical showcase wall
- **THEN** its project support constraint SHALL limit inward planar displacement at the configured compression distance
- **AND** tangent motion, vertical movement and moving away SHALL remain permitted
- **AND** the motor SHALL stop reapplying inward acceleration while that contact persists.

#### Scenario: Dynamic characters survive viewport suspension
- **WHEN** a window resize temporarily suspends the viewport while dynamic characters change transforms or mesh versions
- **THEN** renderer resource subscriptions SHALL follow those binding updates even while no output can be prepared
- **AND** restoring the viewport SHALL render the characters with their existing material payloads without resetting the scene or republishing unchanged assets.

### Requirement: Model-specific World2 spring demonstration

The project SHALL provide a separate World2 scene with the imported penguin, goose and peacock bind meshes, defaulting to penguin. Static actors SHALL load through World, and runtime characters SHALL use model-specific spring controls and constraints derived from skin influence regions and skeleton topology.

#### Scenario: Model selection and constraints
- **WHEN** the user presses 5, 6 or 7 in World2
- **THEN** the project SHALL select penguin, goose or peacock and rebuild the current demonstration with that model's capsule and probe dimensions
- **AND** debug display SHALL show structural springs, bend/support links and control particles
- **AND** the existing pair, wall and landing showcases and reflected tuning panel SHALL remain usable.

#### Scenario: Manual jump takes control
- **WHEN** Space or WASD reaches gameplay while automatic replay is active
- **THEN** input SHALL switch to manual control without resetting the character
- **AND** a grounded, controlled character SHALL execute a Space jump
- **AND** focused property editors SHALL retain their keyboard input.

#### Scenario: Peacock abdomen and head probes
- **WHEN** the peacock is selected in World2
- **THEN** it SHALL have an abdomen probe and a separate head probe fitted to the actual head influence region
- **AND** the head probe SHALL follow its simulated control, constrain local head deformation against scene boxes and other-character probes, and avoid duplicate whole-body impulses
- **AND** PROBE and ALL debug modes SHALL distinguish abdomen and head spheres
- **AND** selecting another model SHALL recreate probes using the selected model's profile.

#### Scenario: Original Party model in World2
- **WHEN** the user presses 8 in World2
- **THEN** the original Party mesh SHALL use its seven-point deformation solver, original capsule and abdomen probe
- **AND** pair, wall, landing, live tuning and debug display SHALL remain usable
- **AND** switching back to a bird SHALL restore that bird's rig and probes without stale state.

#### Scenario: Penguin and goose head contact
- **WHEN** the penguin or goose is selected
- **THEN** it SHALL have a separate head probe fitted to its visible head influence region, including a local center offset where needed
- **AND** the offset SHALL be included in following motion and node contact conversion
- **AND** head contact SHALL constrain local rig deformation without an additional whole-body impulse.

#### Scenario: Goose segmented neck contact
- **WHEN** the goose is selected
- **THEN** one middle-neck probe and one head probe SHALL follow their simulated controls and be visible in PROBE/ALL debug modes, with the abdomen probe kept separately
- **AND** scene and other-character contacts SHALL constrain the corresponding local controls without additional main-body impulses
- **AND** same-character probe overlaps SHALL be ignored and model changes SHALL remove all old neck triggers.

#### Scenario: Jelly-like movement response
- **WHEN** the goose accelerates, stops or reverses
- **THEN** its softer neck SHALL visibly lag, swing back and decay within the existing constraints
- **AND** the reflected Rig inertia value SHALL scale acceleration-driven rig response live
- **AND** the rigid-body bounce setting SHALL remain independent and steady translation SHALL not add a scripted perpetual sway.

#### Scenario: Fitted bird wall contact
- **WHEN** a World2 bird presses against the static wall
- **THEN** its abdomen cage SHALL convert contacts and deformation through that model's probe center and radius
- **AND** the existing head/neck probes SHALL bound motor-driven compression using rest-frame support while preserving sliding, escape and jumping
- **AND** positional depenetration SHALL NOT create an additional outward launch velocity
- **AND** the goose default response SHALL retain a small decaying movement lag, with one head sphere and one neck sphere.
