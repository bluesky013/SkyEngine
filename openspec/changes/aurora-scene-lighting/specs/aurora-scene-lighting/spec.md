## ADDED Requirements

### Requirement: Pass-owned scene resource declarations
Scene light, shadow and environment shader inputs SHALL be declared in their consuming Pass shared header under Global set 0, Pass set 1, Material set 2 and Batch set 3. PreparedRaster SHALL remain responsible for geometry and exact-version resolution only.

#### Scenario: Missing scene input
- **WHEN** an active shader requires a scene resource without an explicit binding or documented default
- **THEN** composition returns an explicit error before native encoding

### Requirement: Depth-only shadow production and sampling
The graph SHALL support validated single-sample D32 depth-only raster and subsequent depth sampling, preserving aspect, content and dependency rules across supported backends.

#### Scenario: Read after shadow write
- **WHEN** a shadow pass clears/writes D32 and the scene pass samples its declared range
- **THEN** the graph preserves the producer and native synchronization makes those depth values visible

#### Scenario: Undefined or incompatible depth read
- **WHEN** a sampled depth range is undefined or the shader view type does not match
- **THEN** the graph rejects the pass before submission

### Requirement: Scene lighting and caster independence
The scene pipeline SHALL consume bounded direct-light values and a directional shadow map from owned snapshots. A shadow caster SHALL NOT be discarded solely because it is outside the main camera frustum.

#### Scenario: Off-camera caster
- **WHEN** a caster outside the camera frustum lies between the shadow light and a visible receiver
- **THEN** its shadow changes the receiver's pixels

### Requirement: HDR and environment binding
The scene pipeline SHALL retain linear HDR color before final exposure/tone mapping and SHALL bind exact environment resource versions with explicit neutral defaults. Unsupported environment shapes SHALL return errors.

#### Scenario: Environment replacement while a frame is in flight
- **WHEN** a newer environment version is published while an older frame uses the previous version
- **THEN** both frames retain their exact inputs until retirement

#### Scenario: No environment input
- **WHEN** an environment asset is absent
- **THEN** the documented default inputs produce defined pixels without missing descriptor accesses

### Requirement: Independent native verification
Scene lighting SHALL be verified with graph failure tests and actual pixel readback on each available backend mode. Unexecuted native platforms SHALL remain explicitly unverified.

#### Scenario: Windows verification
- **WHEN** Windows validation completes
- **THEN** evidence distinguishes Vulkan dynamic rendering, Vulkan render passes and DX12, and does not imply Metal execution
