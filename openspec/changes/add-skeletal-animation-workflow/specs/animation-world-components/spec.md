## ADDED Requirements

### Requirement: Reflected animation component configuration
The system SHALL provide an Actor AnimationComponent with explicit skeleton identity, Clip or Graph source mode, source UUID, autoplay, finite nonnegative global playback rate, RootPolicy and typed parameter defaults. Component loop SHALL affect Clip mode only; Graph Clip nodes SHALL own their loop settings. Configuration SHALL serialize with the World and use the reflected inspector. Playback time, requests, node pointers and pose buffers SHALL NOT be serialized. The first version SHALL allow at most one animation instance per Actor and SHALL NOT own Mesh/Skin assets.

#### Scenario: Save and reopen a configured actor
- **WHEN** a World containing a configured AnimationComponent is saved and reopened
- **THEN** asset bindings and configured defaults SHALL be restored and runtime playback SHALL initialize from those defaults rather than stale pointers or prior execution storage

#### Scenario: Invalid playback configuration
- **WHEN** a negative or non-finite playback rate is applied
- **THEN** the component SHALL return an explicit error and preserve its last valid configuration

### Requirement: World owns animation evaluation
A World animation system SHALL own instance lifecycles and evaluate each registered instance at most once per World tick. It SHALL complete accepted CPU evaluation and publish poses before its tick returns, allowing scene extraction after World::Tick. It SHALL NOT depend on Actor component iteration order, advance physics implicitly, or create a renderer.

#### Scenario: Component ordering does not affect the pose
- **WHEN** the same actor's components are inserted in different orders and the World advances by the same deltas
- **THEN** its published pose SHALL be identical and SHALL have advanced only once per tick

#### Scenario: Delete during pending work
- **WHEN** an actor, component, or World is removed with asset requests or CPU work pending
- **THEN** its generation SHALL be invalidated, accepted work SHALL be collected before owned state is released, and late results SHALL NOT recreate the instance

### Requirement: Explicit playback and root pose controls
Clip mode SHALL support Play/Pause/Stop/Seek/loop/rate with explicit status. Pause SHALL preserve time; Stop SHALL reset to zero and stop; finite Seek SHALL clamp to [0,duration], immediately evaluate and preserve playing/paused state. Loop advancement SHALL wrap into [0,duration); explicit Seek(duration) SHALL display the endpoint until positive advancement. Graph SHALL support Play/Pause/Stop but return Unsupported for Seek; Graph Stop SHALL reset entry state, node times and transitions while retaining current parameters. Root policy SHALL be KeepInPose or LockToReference and SHALL NOT move the Actor. Negative/nonfinite deltas SHALL fail without advancing.

#### Scenario: Pause and scrub
- **WHEN** a playing Clip is paused, the World ticks, and the user seeks to a valid time
- **THEN** time SHALL remain unchanged during the paused tick, the seek pose SHALL be published, and playback SHALL remain paused

#### Scenario: Root motion is not implicitly applied
- **WHEN** a Clip with root translation is played under either supported root policy
- **THEN** the Actor transform SHALL remain unchanged, with root animation retained or locked in the pose according to the selected policy

#### Scenario: Graph seek is unsupported
- **WHEN** a Graph instance receives Seek
- **THEN** it SHALL return Unsupported and preserve all node/state clocks

### Requirement: Renderer-neutral asset and pose interfaces
Clip and skeleton asset access SHALL use consumer-owned interfaces in engine/animation. Aurora SHALL consume owned model-space joint matrices through its own ISkeletalPoseSource interface without including or linking animation plugin implementation types. Service calls SHALL occur on the owning thread while the provider is alive; render packets SHALL NOT retain provider, World, Actor, animation-node, or asset-handler pointers.

#### Scenario: Headless evaluation
- **WHEN** valid clip and skeleton providers are supplied without a renderer service
- **THEN** animation evaluation SHALL succeed and produce renderer-neutral output

#### Scenario: Provider removal
- **WHEN** a module unregisters a provider after draining its accepted work
- **THEN** later consumers SHALL observe an explicit unavailable state and existing owned packets SHALL remain valid

### Requirement: Instance and binding isolation
Instances SHALL own independent playback, parameters, state machines, mutable plans, pose pools and revisions. Animation binding SHALL validate skeleton identity/signature against Clip/Graph assets and publish that identity with its Pose. Aurora SHALL separately validate Mesh/Skin/skeleton combinations and pose-source agreement; the animation system SHALL NOT depend on those rendering assets. Shared immutable assets SHALL NOT contain mutable per-instance poses.

#### Scenario: Two characters share assets
- **WHEN** two actors use the same mesh, skeleton and graph but different parameters or playback times
- **THEN** they SHALL produce independent poses without mutating one another's state

#### Scenario: Same UUID with changed rig layout
- **WHEN** reloaded skeleton data has the same UUID but a different layout signature
- **THEN** affected instances SHALL revalidate and rebuild bindings before publishing any pose under the new signature

### Requirement: Atomic loading and replacement
Loading SHALL be nonblocking at UI and World tick boundaries. Results SHALL match the current request generation. Animation first-load failures SHALL report an error; Aurora may display a clearly identified reference pose only when its Mesh/Skin/rig binding is valid. A same-binding animation reload SHALL replace the complete rig/clip/graph instance atomically or preserve its previous complete version with a diagnostic; Aurora SHALL independently retain a coherent render snapshot. An explicitly changed binding SHALL NOT masquerade as the previous binding.

#### Scenario: Out-of-order asset responses
- **WHEN** a component changes its binding twice and the earlier request completes last
- **THEN** the earlier response SHALL be discarded and SHALL NOT change the new instance

#### Scenario: Invalid reload preserves a complete version
- **WHEN** a replacement graph or clip fails validation for an existing binding
- **THEN** the previous complete instance SHALL remain usable, and its old revision and reload error SHALL be observable

### Requirement: Editor playback uses a domain control interface
Animation.Editor SHALL control instances through engine/animation's IAnimationInstanceControl with World epoch, Actor identity and instance generation. The World adaptor SHALL implement the interface and explicit command/status results. Editors SHALL NOT include World adaptor implementation headers or bypass the instance lifecycle to mutate node/player pointers. Debug state SHALL include graph revision and source-node identities.

#### Scenario: Stale preview control
- **WHEN** a queued preview command targets a destroyed or recreated instance generation
- **THEN** it SHALL return a stale-target result without changing the replacement instance
