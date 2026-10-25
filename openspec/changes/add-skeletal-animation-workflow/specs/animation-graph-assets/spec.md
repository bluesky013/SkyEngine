## ADDED Requirements

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
