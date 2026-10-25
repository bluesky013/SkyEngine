## MODIFIED Requirements

### Requirement: Runtime animation data construction reports validation failures
Skeleton, channel and clip construction SHALL validate bone counts, unique identities, parent bounds and cycles, reference-pose sizes, finite invertible transforms, key array shapes and times, explicit clip length and named channel compatibility before evaluation. Invalid runtime input SHALL return an explicit status rather than throw, assert or silently omit a channel.

#### Scenario: Invalid topology or channel data
- **WHEN** a rig has a parent cycle or a clip channel has mismatched key arrays or an unknown bone
- **THEN** construction or binding SHALL fail before invalid data is used for sampling

### Requirement: Sampling restores unanimated reference channels
Each Clip sampling operation SHALL initialize its destination from the selected skeleton's reference local transforms before applying available channels. Valid partial clips SHALL leave missing joints and missing TRS components at reference values. Unknown named channels SHALL remain an error on the validated asset path. Plan and virtual evaluation SHALL follow the same rule.

#### Scenario: Reused workspace with partial animation
- **WHEN** one evaluation animates a joint's translation and the next clip omits that channel
- **THEN** the next pose SHALL restore the reference translation without retaining the previous clip's value

### Requirement: Clip duration reflects key times
An animation clip SHALL preserve an authoritative source frame count when supplied, including after key reduction. Without that metadata it SHALL derive frame count from the last key time across channels plus one. A supplied frame count SHALL cover every key. Duration SHALL be frame count divided by positive frame rate, independent of retained key count and adaptor loading. Zero frame rate SHALL produce zero duration without time advancement.

#### Scenario: Sparse clip duration
- **WHEN** a clip without explicit length has its last key at frame N and frame rate F
- **THEN** its duration SHALL be (N + 1) / F

#### Scenario: Duration without an externally supplied frame count
- **WHEN** a clip is built from channels and a frame rate only
- **THEN** its duration SHALL match (lastKey + 1) / frameRate

#### Scenario: Zero frame rate
- **WHEN** the frame rate is zero
- **THEN** the duration SHALL be zero and playback SHALL NOT advance

#### Scenario: Constant tail survives compression
- **WHEN** key reduction removes duplicate keys from a constant clip or its final static interval
- **THEN** the cooked and runtime clip SHALL preserve the original source frame count and end time

#### Scenario: Invalid explicit length
- **WHEN** supplied source length ends before a channel's final key
- **THEN** source validation SHALL fail with a diagnostic rather than truncate playback

### Requirement: Clip node playback control
A clip node SHALL bind a clip to the graph and expose playback, looping, and root-motion controls. Sampling SHALL position the clip at the player's current time. The virtual graph and lowered-plan paths SHALL apply updated controls consistently before advancement; initialization SHALL honor the configured playing flag. Seek SHALL update sampling time without implicitly starting playback.

#### Scenario: Sampling follows playback time
- **WHEN** a clip node with a playing clip is evaluated after time has advanced
- **THEN** the sampled pose SHALL correspond to the player's current clip time

#### Scenario: Non-looping clip holds the final pose
- **WHEN** a non-looping clip node is evaluated after reaching the clip end
- **THEN** it SHALL sample the clip at its end time

#### Scenario: Looping clip continues
- **WHEN** a looping clip node passes the clip end and is evaluated
- **THEN** it SHALL sample within the clip duration and remain playing

#### Scenario: Paused initialization and plan controls
- **WHEN** a paused clip node is initialized or its playing/looping flags change in a lowered plan
- **THEN** its next advance SHALL honor those flags without relying on an uncalled virtual PreTick
