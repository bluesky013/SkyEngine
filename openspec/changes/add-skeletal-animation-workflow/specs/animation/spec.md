## ADDED Requirements

### Requirement: Independent mutable evaluation state

Mutable node bindings, players, pose pools and frame state SHALL be isolated per animation instance. An AnimationPlan containing mutable evaluation storage SHALL NOT be evaluated as a shared multi-instance program. Stable instances SHALL reuse their preallocated evaluation storage.

#### Scenario: Concurrent independent evaluations
- **WHEN** separate instances evaluate the same immutable graph definition at different times
- **THEN** they SHALL use disjoint mutable workspaces and produce the same results as separate serial evaluations

### Requirement: Invalid rig and clip input reports failure

The touched asset-to-animation creation path SHALL validate bone counts, unique identities, parent bounds/cycles, reference-pose sizes, finite transforms, key array shapes and times before constructing runtime objects. Recoverable invalid data SHALL return explicit failure and SHALL NOT be handled by assertions, exceptions or silent channel omission.

#### Scenario: Malformed rig or clip
- **WHEN** an asset has a parent cycle, a missing reference transform, a mismatched key array or an unknown bone channel
- **THEN** binding SHALL fail with an asset-specific diagnostic before pose evaluation accesses the data

### Requirement: Production-path isolated animation preview

The editor SHALL provide a separate Preview World for skeletal mesh, Clip and Graph inspection using the production components and skinning path. Its scene, view and output SHALL use the existing renderer host and packet producer, without creating another Device or renderer. Preview time and parameter changes SHALL NOT mutate the editing World or PIE instance.

#### Scenario: Independent preview
- **WHEN** a user changes preview time or parameters while the same asset is used in the editing World
- **THEN** only the Preview World instance SHALL change

#### Scenario: Close with frames in flight
- **WHEN** a preview window is closed while its output or palettes are in flight
- **THEN** logical teardown SHALL begin immediately and native/output resources SHALL be released only after the existing retirement protocol permits it

### Requirement: Skeleton and clip inspection

The preview SHALL provide independent Mesh/Skin/Skeleton selection, resolved binding diagnostics, skeleton tree, selected-bone transform, bone/bounds display and Play/Pause/Stop/rate controls. Clip preview SHALL provide loop and scrub; Graph preview SHALL use its node loop settings and SHALL NOT expose whole-graph Seek. Pending assets and failures SHALL be visible. Paused Clip scrubbing SHALL use the runtime evaluation path.

#### Scenario: Scrub a known two-bone clip
- **WHEN** the timeline is moved to a known sample time while paused
- **THEN** the displayed mesh, bone transform and bounds SHALL correspond to that pose and playback SHALL remain paused

#### Scenario: Preview another binding of the same mesh
- **WHEN** the user keeps the Mesh selection and changes Skin/Skeleton
- **THEN** the preview SHALL validate and display the selected combination or identify the incompatible slot without mutating other instances

### Requirement: Reusable graph canvas with domain-owned semantics

Sandbox SHALL provide a domain-neutral graph canvas for zoom/pan, node movement, selection, typed-pin interaction, connection display and editing actions. The animation plugin SHALL provide node definitions, validation and state-machine semantics. Runtime and builder targets SHALL NOT depend on canvas or editor modules.

#### Scenario: Canvas edits a graph
- **WHEN** nodes are moved or connected through the canvas
- **THEN** the document SHALL receive stable-identity editing commands and the canvas SHALL NOT instantiate animation runtime objects directly

### Requirement: Reflected and undoable graph editing

Graph documents SHALL use CommandService for node, connection, parameter, transition and layout edits; a drag SHALL be one undo transaction. Node/settings properties SHALL use ReflectedFormView or descriptor-provided reflected objects. Menus, toolbars and shortcuts SHALL use EditorActionRegistry; dimensions/colors SHALL use UiTheme and UiSkin.

#### Scenario: Undo a node deletion
- **WHEN** a node with links is deleted and the action is undone
- **THEN** its identity, properties, position and links SHALL be restored together

#### Scenario: Reflected parameter edit
- **WHEN** a graph default is edited in the reflected form and undone
- **THEN** both the document value and preview compilation input SHALL return to their previous value

### Requirement: Deferred save and honest preview versions

Source edits SHALL remain buffered until Save. Save SHALL resolve paths through AssetDataBase, write a serializable schema-valid source atomically once, and enqueue deferred cook. Incomplete executable graphs SHALL remain saveable drafts and SHALL fail cook with diagnostics. UI dispatch SHALL NOT cook or synchronously rebuild/destroy its active widget. Saved-source, cook and preview revisions SHALL be tracked separately. v1 preview SHALL instantiate the last successful cooked product through the product loader; unsaved drafts SHALL receive structural diagnostics but SHALL NOT masquerade as a new running product.

#### Scenario: Save then cook fails
- **WHEN** a source save succeeds but asynchronous cook fails
- **THEN** the source SHALL remain saved, the cook diagnostic SHALL be visible, and any retained old preview SHALL be identified by its old revision

#### Scenario: Save during pointer dispatch
- **WHEN** a Save action originates from a pointer event
- **THEN** heavy work SHALL be deferred until dispatch completes and the active widget SHALL remain valid for the rest of the event

#### Scenario: Save unfinished graph work
- **WHEN** a serializable graph has no root output and the user saves it
- **THEN** the document SHALL reopen with the unfinished edits preserved, cook SHALL report the missing output, and any old preview SHALL remain explicitly identified as old

### Requirement: Graph diagnostics and runtime inspection

The Graph editor SHALL display parameter values, active state, transition progress and diagnostics mapped to source node/pin identities. Temporary debug parameter overrides SHALL be distinct from persisted defaults. Unsupported nested graphs and root-motion extraction SHALL be diagnosed rather than silently approximated.

#### Scenario: Usable viewport and preview setup
- **WHEN** a Graph opens or its window is resized
- **THEN** the canvas and selected inspector SHALL fit the available viewport, with Graph, selected-node and preview settings accessible through scoped action tabs
- **AND** an unavailable preview SHALL show an in-viewport diagnostic and setup guidance rather than an unexplained empty rectangle

#### Scenario: Scale or frame a Graph
- **WHEN** DPI changes or the user frames all nodes
- **THEN** logical node positions and connections SHALL remain unchanged, drawing and hit-testing SHALL use the same scale, and framing SHALL update only the canvas view
- **AND** nodes SHALL prefer readable asset/state names over UUID-only titles

#### Scenario: Mesh becomes ready after its skeleton
- **WHEN** a configured preview Mesh finishes loading after the Skeleton
- **THEN** initial camera framing SHALL wait for Mesh bounds and fit them using the viewport aspect ratio and field of view
- **AND** loading diagnostics SHALL remain readable without bone or bounds overlays obscuring them

#### Scenario: Locate a compile failure
- **WHEN** graph compilation reports an invalid connection
- **THEN** the editor SHALL identify the owning node/pin and preserve the editable document

### Requirement: Honest skinned selection capabilities

Until deformation-aware triangle picking and highlighting are implemented, the editor SHALL use Outliner or explicitly identified conservative bounds selection for skinned objects. It SHALL NOT reuse bind-pose triangle picking or positions-preserved static highlighting as if accurate for the animated pose.

#### Scenario: A limb leaves the bind pose
- **WHEN** a skinned limb moves outside its original mesh bounds
- **THEN** animated bounds SHALL remain visible/selectable and the editor SHALL NOT display an incorrect bind-pose selection outline

### Requirement: Versioned graph source and product

An AnimationGraph SHALL have a versioned JSON source containing stable node, parameter, state and transition identities, skeleton UUID, typed connections, a root output, and editor layout stored separately from executable semantics. Its cooked definition SHALL contain pose topology, parameter slots, clip dependencies, state/condition/transition control data, and source-node diagnostic mapping. It SHALL NOT consist solely of AnimationPlanBlob or a version number.

#### Scenario: Source round trip
- **WHEN** a graph is saved and reopened after a node is renamed or moved
- **THEN** connections and semantic identities SHALL remain valid, layout SHALL be restored, and only semantic changes SHALL affect the executable definition

#### Scenario: Product contains state control
- **WHEN** a graph with Idle and Walk states is cooked and loaded from products
- **THEN** parameters, conditions, entry state and transitions SHALL remain available without reading the source JSON

### Requirement: Bounded node and state model

The first graph version SHALL support Clip, Blend2 and Output pose nodes, bool/float parameters, and at most one top-level flat state machine whose states own pose trees. Pose connections SHALL be typed and acyclic. A stateful pose node SHALL have only one owning evaluation path; parameter reads may be shared. State-transition cycles SHALL be allowed. Nested or multiple state machines SHALL be rejected explicitly.

#### Scenario: Legal locomotion loop
- **WHEN** a graph contains valid Idle-to-Walk and Walk-to-Idle transitions
- **THEN** structural validation SHALL accept the transition cycle

#### Scenario: Illegal pose cycle or unsupported nesting
- **WHEN** pose edges form a cycle, a stateful node has multiple owners, or a graph contains nested state machines
- **THEN** validation SHALL fail with the involved node identities and a specific diagnostic

### Requirement: Deterministic graph validation and cook

The Graph builder SHALL reject duplicate identities/names, unknown node types, missing or mistyped pins, multiple root outputs, missing entry state, invalid defaults/ranges and missing catalog dependencies. It SHALL emit a normalized definition and register skeleton/clip dependencies only after structural validation succeeds. The builder SHALL NOT load a World, renderer, runtime animation adaptor or Content.Adaptor. Runtime binding SHALL additionally validate exact asset contents and rig compatibility before Ready.

#### Scenario: Invalid graph does not replace a product
- **WHEN** an edited graph contains a dangling clip reference or incompatible pin
- **THEN** cook SHALL fail with asset/node/pin context and SHALL NOT publish a new successful graph product

#### Scenario: Structurally valid graph has an incompatible clip
- **WHEN** a cooked graph resolves to a clip whose skeleton signature does not match the bound rig
- **THEN** instance creation SHALL fail before publishing a pose

### Requirement: Per-instance control and plan execution

The runtime SHALL instantiate control state, clip players, typed parameters, evaluation plan and temporary pose storage per graph instance after assets are ready. Shared graph products SHALL remain immutable. Lowering/compilation SHALL occur at creation or rebind, not once per frame. Virtual-node and plan execution SHALL honor the same configured playback controls.

#### Scenario: Shared graph independent state
- **WHEN** two instances of the same graph choose different states
- **THEN** their plan workspaces and control objects SHALL be independent and each result SHALL correspond to its own parameters

#### Scenario: Stable binding avoids recompilation
- **WHEN** an instance advances without an asset or graph revision change
- **THEN** it SHALL reuse its compiled plan and preallocated evaluation storage

### Requirement: Explicit transition behavior

Conditions SHALL use frozen per-tick bool/float parameters, AND semantics within a transition and optional normalized exit time. Higher priority SHALL win, followed by ascending stable identity. At most one transition SHALL start per tick; self-transitions SHALL be rejected in v1. Duration SHALL be finite/nonnegative; zero SHALL switch immediately and positive values SHALL crossfade without interruption. At tick start the runtime SHALL select transitions using prior accumulated clocks, then advance active players/fade by globally rate-scaled delta and publish the pose. Reentry SHALL reset players; inactive states SHALL NOT advance. Pause or global rate zero SHALL freeze clocks/fades/transitions while permitting parameter-driven resampling.

#### Scenario: Crossfade reaches the destination
- **WHEN** a transition of duration T is advanced by a total of T
- **THEN** both sides SHALL be evaluated during the blend and the destination SHALL become the sole active state at completion

#### Scenario: Simultaneous conditions and inactive players
- **WHEN** several conditions are true while other states remain inactive
- **THEN** the deterministic highest-priority eligible transition SHALL start and unrelated inactive players SHALL retain their times

#### Scenario: Delta exceeds remaining crossfade
- **WHEN** a playing transition has less remaining duration than the next scaled delta
- **THEN** the destination SHALL advance by the full delta, the source SHALL advance only to completion, and no second transition SHALL start that tick

### Requirement: Explicit exit-time clock and pose defaults

An exit-time condition SHALL identify one Clip node owned by its source state as timeSourceNodeId. Normalized time SHALL be that player's non-wrapping elapsed time since entry divided by its positive duration, with an exit threshold in [0,1]. Once reached the time condition SHALL remain true for that state entry; other conditions may become true later. A zero-duration source SHALL allow only threshold zero. A multi-Clip state SHALL NOT infer a clock arbitrarily. Clip sampling SHALL start from a complete reference pose and restore missing joint/TRS channels on every evaluation; Blend2 SHALL clamp finite alpha to [0,1] and blend local TRS with normalized shortest-arc rotations.

#### Scenario: Blend state has two clip durations
- **WHEN** a Blend2 state uses different Clip durations and an exit-time condition without timeSourceNodeId
- **THEN** validation SHALL fail with a clock diagnostic rather than choose an implicit duration

#### Scenario: Loop crosses exit threshold with a large delta
- **WHEN** the designated clock crosses the exit threshold and loops several times during a tick
- **THEN** the non-wrapping clock SHALL retain the crossing and the next tick's transition check SHALL observe it

#### Scenario: Partial clips use clean defaults
- **WHEN** successive states sample clips that animate different subsets of joints or TRS channels
- **THEN** unanimated values SHALL come from the skeleton reference pose rather than stale workspace contents

### Requirement: Single graph registration owner and migration

The animation plugin SHALL be the sole owner of the AnimationGraph handler, `.graph` builder and Graph editor. Content SHALL stop registering its placeholder graph path. Existing source UUIDs SHALL be preserved. Version-only products SHALL be rejected with a recook diagnostic; an empty old source SHALL open as an incomplete document requiring a valid output before successful cook.

#### Scenario: Module load order does not select a different handler
- **WHEN** Content and animation modules are loaded in either valid dependency order
- **THEN** exactly one AnimationGraph handler and builder SHALL own the type and neither SHALL overwrite another live registration

#### Scenario: Old empty graph
- **WHEN** an old empty `.graph` is opened and cooked without adding nodes
- **THEN** the editor SHALL show an incomplete document and cook SHALL report a missing output rather than creating a successful empty runtime graph

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
