## ADDED Requirements

### Requirement: Runtime animation module boundary
The animation runtime SHALL be provided by the `Animation` module, which SHALL depend only on `Core` and SHALL NOT depend on rendering modules. Rendering and editor consumers SHALL interact with it through its public runtime types.

#### Scenario: No render dependency
- **WHEN** the `Animation` module is built
- **THEN** it SHALL link only against `Core` and its own dependencies, with no render target in its link interface

### Requirement: Animation instance lifecycle
An animation instance SHALL own a node graph and named parameters, SHALL be initialized with a root node and skeleton, and SHALL advance through an update phase and an evaluation phase. The update phase SHALL advance graph state by delta time; the evaluation phase SHALL sample the graph into a pose without advancing time.

#### Scenario: Update then evaluate
- **WHEN** an initialized instance is ticked with a delta time and then evaluated
- **THEN** the graph SHALL have advanced by that delta and the evaluation SHALL sample the advanced state

#### Scenario: Missing root node
- **WHEN** an instance with no root node is evaluated
- **THEN** evaluation SHALL produce the skeleton reference pose instead of failing

#### Scenario: Evaluation does not advance time
- **WHEN** an instance is evaluated repeatedly without an intervening tick
- **THEN** each evaluation SHALL produce the same pose and SHALL NOT advance graph time

### Requirement: Named animation parameters
An animation instance SHALL store named parameters that are updated once per tick, and transition conditions SHALL read the current parameter value during evaluation.

#### Scenario: Parameter updated per tick
- **WHEN** a parameter is registered and the instance is ticked
- **THEN** the parameter value SHALL reflect the tick delta before conditions are evaluated

#### Scenario: Supported value types
- **WHEN** a bool or float parameter is registered and updated
- **THEN** reading it as its declared type SHALL return the cached value

#### Scenario: Unavailable parameter
- **WHEN** a condition references a parameter that is not available
- **THEN** the condition SHALL evaluate false rather than read stale or invalid data

### Requirement: Node graph and layer weighting
The animation graph SHALL be composed of nodes implementing the node contract (initialize, pre-tick, tick, evaluate) and SHALL propagate a layer context carrying a normalized weight and bone mask to child nodes.

#### Scenario: Layer weight propagation
- **WHEN** a node evaluates through a layer context with weight w
- **THEN** its children SHALL receive a context whose weight is w multiplied by the layer's own weight

#### Scenario: Nested layer weights
- **WHEN** layers with weights are nested
- **THEN** the effective weight reaching a leaf node SHALL be the product of the ancestor weights

### Requirement: Skeleton model
A skeleton SHALL represent a bone hierarchy with named bones, parent/child relationships, and a shared reference pose, and SHALL resolve a bone by name to its index.

#### Scenario: Resolve bone by name
- **WHEN** a bone name present in the skeleton is looked up
- **THEN** the corresponding bone index SHALL be returned

#### Scenario: Unknown bone name
- **WHEN** an unknown bone name is looked up
- **THEN** the lookup SHALL report no bone instead of an invalid index

#### Scenario: Hierarchy construction
- **WHEN** a skeleton is built from bone parent indices
- **THEN** each bone's children SHALL contain its direct children and the roots SHALL be the bones with no parent

#### Scenario: Reference pose size
- **WHEN** a skeleton is built
- **THEN** its shared reference pose SHALL provide one transform per bone

### Requirement: Pose representation
An animation pose SHALL store a transform per bone, a bone mask selecting participating bones, and a reference to its skeleton. A pose SHALL be resettable to the skeleton reference pose, SHALL support rotation normalization, and SHALL convert to per-bone skin matrices using a root transform.

#### Scenario: Reset to reference pose
- **WHEN** a pose is reset against its skeleton
- **THEN** its transforms SHALL equal the skeleton reference pose

#### Scenario: Reset without a skeleton
- **WHEN** a pose with no skeleton reference is reset
- **THEN** it SHALL be left unchanged instead of failing

#### Scenario: Rotation normalization
- **WHEN** a pose's rotations are normalized
- **THEN** every transform rotation SHALL be unit-length

#### Scenario: Skin matrices
- **WHEN** a pose is converted to skin render data with a root transform
- **THEN** the output SHALL contain one matrix per bone, composed through the bone hierarchy and the root transform

#### Scenario: Parent transforms affect children
- **WHEN** a pose with differing parent and child transforms is converted to skin matrices
- **THEN** each bone's matrix SHALL include its ancestors' transforms

### Requirement: Clip channel data
An animation clip SHALL store channels keyed by bone name. Each channel SHALL store per-component keyframe data with monotonically ordered key times, SHALL support compressing redundant keys, and SHALL report the keyframe span containing a sample time.

#### Scenario: Compress redundant keys
- **WHEN** consecutive keys differ by less than the difference tolerance
- **THEN** compression SHALL drop the redundant keys while preserving the first and last keys

#### Scenario: Keyframe span lookup
- **WHEN** a sample time falls between two keys
- **THEN** the lookup SHALL return the bounding pair of key indices

#### Scenario: Out-of-range sample time
- **WHEN** a sample time is before the first key or after the last key
- **THEN** the lookup SHALL clamp to the first or last key respectively

#### Scenario: Single key
- **WHEN** a component has exactly one key
- **THEN** sampling at any time SHALL return that key's value

#### Scenario: Exact key time
- **WHEN** a sample time equals a key time
- **THEN** the sampled value SHALL equal that key's value

#### Scenario: Resize allocates matching storage
- **WHEN** a channel component is resized to N
- **THEN** its times and keys SHALL both provide N entries

### Requirement: Channel sampling applies position, rotation, and scale
Clip sampling SHALL apply the position, rotation, and scale components that are present in the channel. Step interpolation SHALL hold the previous key; linear interpolation SHALL interpolate scalars and vectors and SHALL spherical-interpolate rotations along the shortest arc. A component with no keys SHALL leave the incoming reference value unchanged.

#### Scenario: Step holds previous key
- **WHEN** a component with step interpolation is sampled between two keys
- **THEN** the result SHALL equal the earlier key's value

#### Scenario: Linear midpoint
- **WHEN** a component with linear interpolation is sampled halfway between two keys
- **THEN** the result SHALL be the midpoint of the two key values

#### Scenario: Rotation shortest arc
- **WHEN** a rotation component is sampled between two rotations
- **THEN** the result SHALL be a unit quaternion interpolated along the shortest arc

#### Scenario: Scale is applied
- **WHEN** a channel provides scale keys and the clip is sampled
- **THEN** the destination transform scale SHALL be updated from the sampled scale

#### Scenario: Absent component preserves reference
- **WHEN** a channel has no keys for one component
- **THEN** the destination transform value for that component SHALL remain the reference pose value

#### Scenario: Bone mask excludes a channel
- **WHEN** the evaluation pose's bone mask has the channel's bone bit cleared
- **THEN** the channel SHALL NOT be applied to the pose

### Requirement: Sequence playback timing
A sequence player SHALL advance clip time by play rate and delta time, SHALL support looping and non-looping modes, and SHALL use the clip duration derived from its key times. A non-looping player SHALL stop when time reaches the duration; a looping player SHALL wrap time within the duration.

#### Scenario: Non-looping stop
- **WHEN** a non-looping player is advanced beyond the clip duration
- **THEN** playback SHALL stop at the clip duration

#### Scenario: Looping wrap
- **WHEN** a looping player is advanced beyond the clip duration
- **THEN** playback time SHALL wrap within the clip duration and remain playing

#### Scenario: Play rate scales advance
- **WHEN** a player with a play rate of R is advanced by a delta
- **THEN** its clip time SHALL advance by R multiplied by the delta

#### Scenario: Reverse playback clamps at zero
- **WHEN** a non-looping player is advanced below zero time
- **THEN** its time SHALL clamp to zero

### Requirement: Clip duration reflects key times
An animation clip SHALL derive its duration from the last key time across its channels, without relying on an externally supplied frame count, so that non-looping playback stops and looping playback wraps at the clip's real length regardless of the asset or adaptor layer it is loaded through.

#### Scenario: Sparse clip duration
- **WHEN** a clip's last key occurs at frame N with frame rate F
- **THEN** the clip duration SHALL be (N + 1) / F

#### Scenario: Duration without an externally supplied frame count
- **WHEN** a clip is built from channels and a frame rate only
- **THEN** its duration SHALL match (lastKey + 1) / frameRate

#### Scenario: Zero frame rate
- **WHEN** the frame rate is zero
- **THEN** the duration SHALL be zero and playback SHALL not advance

### Requirement: Pose blending uses the source pose
Pose blending SHALL consume both the source and destination poses. Override blending SHALL interpolate the destination toward the source by weight; additive blending SHALL accumulate a weighted delta onto the destination. Rotations SHALL be blended as rotations and SHALL remain unit-length.

#### Scenario: Override halfway
- **WHEN** override blending is applied with weight 0.5 between differing destination and source transforms
- **THEN** the destination transform SHALL be the midpoint between the two

#### Scenario: Override at full weight
- **WHEN** override blending is applied with weight 1
- **THEN** the destination transform SHALL equal the source transform

#### Scenario: Override at zero weight
- **WHEN** override blending is applied with weight 0
- **THEN** the destination transform SHALL be unchanged

#### Scenario: Additive accumulation
- **WHEN** additive blending is applied with weight less than 1
- **THEN** the destination SHALL be shifted by the weighted delta and SHALL remain a valid transform with a unit rotation

#### Scenario: Additive at zero weight
- **WHEN** additive blending is applied with weight 0
- **THEN** the destination transform SHALL be unchanged

#### Scenario: Mismatched pose sizes
- **WHEN** blending is attempted between poses with different transform counts
- **THEN** the destination pose SHALL be left unmodified

### Requirement: Pose blend nodes are weight-driven
Blend nodes SHALL produce an output pose reflecting their configured weights, SHALL reset the destination pose to the reference pose before blending, and SHALL NOT access storage outside their allocated entries.

#### Scenario: Two-pose blend at alpha
- **WHEN** a two-pose blend evaluates with blend alpha 0.5 and different source poses
- **THEN** the result SHALL be the interpolation of the two source poses at 0.5

#### Scenario: Zero-weight entries
- **WHEN** a pose list contains entries with zero weight
- **THEN** evaluation SHALL include the remaining entries at their weights without accessing unallocated entries

#### Scenario: Blend activation
- **WHEN** blending is enabled on a two-pose node and time is advanced
- **THEN** the blend alpha SHALL move from the current pose toward the target pose over the configured blend time

#### Scenario: Blend completes over the blend time
- **WHEN** a two-pose node with a blend time of T is advanced by T while blending toward the target
- **THEN** the blend alpha SHALL reach the target

#### Scenario: Disabling blend returns to the first pose
- **WHEN** blending is disabled on a two-pose node and time is advanced
- **THEN** the output SHALL return to the first pose

### Requirement: Animation state machine
An animation state machine SHALL hold named states, an entry state, transitions guarded by conditions between states, and SHALL evaluate the active state's node each tick. State selection SHALL be bounds-safe and SHALL only follow registered transitions.

#### Scenario: Entry state
- **WHEN** a state machine with a configured entry state is initialized
- **THEN** the active state SHALL be the entry state

#### Scenario: Default entry state
- **WHEN** a finalized state machine has no explicitly configured entry state
- **THEN** the first registered state SHALL be the entry state

#### Scenario: Valid transition
- **WHEN** a registered transition's condition evaluates true
- **THEN** the machine SHALL move to the transition's next state

#### Scenario: Condition false
- **WHEN** no registered transition from the current state has a true condition
- **THEN** the machine SHALL remain in the current state

#### Scenario: At most one transition per tick
- **WHEN** multiple transitions from the current state have true conditions in a single tick
- **THEN** the machine SHALL take a single transition and evaluate the resulting state's node

#### Scenario: Transition from a non-current state is ignored
- **WHEN** a transition whose source is not the current state has a true condition
- **THEN** the machine SHALL remain in the current state

#### Scenario: Re-selecting the current state
- **WHEN** the current state is selected again
- **THEN** it SHALL NOT be re-initialized

#### Scenario: Out-of-range state handle
- **WHEN** a state handle outside the registered range is selected
- **THEN** no out-of-range state SHALL be accessed

### Requirement: Clip node playback control
A clip node SHALL bind a clip to the graph and expose playback, looping, and root-motion controls. Sampling SHALL position the clip at the player's current time.

#### Scenario: Sampling follows playback time
- **WHEN** a clip node with a playing clip is evaluated after time has advanced
- **THEN** the sampled pose SHALL correspond to the player's current clip time

#### Scenario: Non-looping clip holds the final pose
- **WHEN** a non-looping clip node is evaluated after reaching the clip end
- **THEN** it SHALL sample the clip at its end time

#### Scenario: Looping clip continues
- **WHEN** a looping clip node passes the clip end and is evaluated
- **THEN** it SHALL sample within the clip duration and remain playing

### Requirement: Root-motion evaluation is non-destructive
Evaluating a clip with root motion disabled SHALL NOT mutate shared pose state such as the bone mask, and SHALL identify the root bone from the skeleton rather than a hardcoded index. The consumed root delta SHALL be made available on the evaluation context.

#### Scenario: Repeated evaluation preserves bone mask
- **WHEN** a clip with root motion disabled is evaluated more than once
- **THEN** the evaluation pose bone mask SHALL be unchanged between evaluations

#### Scenario: Root bone is the skeleton root
- **WHEN** root motion is disabled for a skeleton whose root is not bone index 0
- **THEN** the root-motion adjustment SHALL be applied to the actual root bone

#### Scenario: Consumed root delta is available
- **WHEN** a clip with root motion is evaluated
- **THEN** the consumed root delta SHALL be available on the evaluation context

### Requirement: Renderer-neutral pose output
The animation core SHALL expose the evaluated final pose to callers without any renderer dependency, so that any renderer adaptor can consume it. Renderer-specific concerns - asset storage, skinning, and debug drawing - SHALL NOT be required for the core to tick and evaluate animation, and the renderer/asset integration SHALL be provided outside the `Animation` module.

#### Scenario: Core evaluates without a renderer
- **WHEN** the animation core is built and evaluates a graph
- **THEN** it SHALL do so without linking or referencing any renderer module

#### Scenario: Pose available to callers
- **WHEN** a caller ticks an instance and evaluates its pose
- **THEN** the evaluated final pose SHALL be obtainable through renderer-neutral core types for downstream skinning or debug rendering

### Requirement: Animation asset builder
The engine SHALL provide an editor/cook-time animation asset builder that converts source animation data into runtime assets for clips, skeletons, and graphs. The builder SHALL validate source data, resolve and register the skeleton dependency, set the clip frame rate, write a versioned runtime asset, and report failures with a diagnostic instead of emitting a partially valid asset. The builder SHALL NOT be required by the runtime.

#### Scenario: Clip build
- **WHEN** the builder receives a valid source clip
- **THEN** it SHALL write a runtime clip asset containing the channels, frame rate, and skeleton reference, and register the skeleton as a dependency

#### Scenario: Invalid source
- **WHEN** a source clip has non-monotonic key times or references a missing skeleton
- **THEN** the builder SHALL fail with a diagnostic and write no asset

#### Scenario: Graph build
- **WHEN** the builder receives a `.graph` source
- **THEN** it SHALL write the runtime graph asset and set the build result

### Requirement: Cook-time curve compression
The builder SHALL compress animation clip keyframe tracks at cook time using ACL v2 (Animation Compression Library), reducing keyframe storage while bounding reconstruction error by a configured threshold. Compression settings - at least the error threshold and compression level - SHALL be configurable per clip with engine defaults.

#### Scenario: Bounded error
- **WHEN** a clip is compressed with a configured error threshold
- **THEN** samples reconstructed from the compressed clip SHALL stay within that threshold of the source track

#### Scenario: Deterministic compression
- **WHEN** the same source clip and settings are compressed twice
- **THEN** the produced compressed data SHALL be identical

#### Scenario: Duration preserved
- **WHEN** a clip is compressed
- **THEN** the last key time SHALL be preserved so the key-derived duration is unchanged

### Requirement: Runtime decompression
The runtime SHALL sample compressed clips through the same channel-sampling interface as uncompressed clips, without requiring the original keyframes, and SHALL depend only on ACL decompression code. Clips stored uncompressed SHALL remain loadable and sampleable.

#### Scenario: Compressed sampling
- **WHEN** a compressed clip is sampled
- **THEN** it SHALL produce values within the configured error tolerance of the source

#### Scenario: Lean runtime dependency
- **WHEN** the animation runtime is linked
- **THEN** it SHALL include ACL decompression only, not the ACL compression and optimizer

#### Scenario: Uncompressed fallback
- **WHEN** a clip is stored uncompressed
- **THEN** the runtime SHALL still sample it correctly

### Requirement: ACL2 third-party dependency
ACL v2 SHALL be provided as a third-party package built through the repository third-party bootstrap (`python/third_party.py`) for all supported platforms, and SHALL be consumed as an external CMake target rather than a vendored copy inside the engine tree.

#### Scenario: Package available
- **WHEN** the third-party bootstrap runs for a target platform
- **THEN** it SHALL build ACL and expose it as a CMake target usable by the animation builder and runtime

#### Scenario: No vendored copy
- **WHEN** the repository is inspected
- **THEN** no ACL sources SHALL be vendored under `engine/`; the third-party package SHALL be the single source

### Requirement: Animation asset versioning and compression metadata
Runtime animation assets SHALL be versioned. The builder SHALL record the asset version and the compression settings with each compressed clip, and the runtime SHALL reject assets whose version or compression information it does not support.

#### Scenario: Version mismatch
- **WHEN** an asset with an unsupported version is loaded
- **THEN** loading SHALL fail with a diagnostic

#### Scenario: Compression metadata
- **WHEN** a compressed clip is written
- **THEN** it SHALL carry the settings required to decompress it
