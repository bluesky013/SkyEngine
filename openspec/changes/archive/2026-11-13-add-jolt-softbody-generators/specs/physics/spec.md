## ADDED Requirements

### Requirement: Portable CPU XPBD softbody simulation
Softbody runtime SHALL execute a backend-independent CPU XPBD solver in the SoftBody implementation plugin. Physics-owned interfaces SHALL contain its signature data and collision query contract. The solver SHALL NOT depend on Jolt or PhysX native softbody actors, CUDA, editor, authoring or rendering implementations. Softbody snapshot identity SHALL be distinct from rigid-world object handles.

#### Scenario: Simulate an all-dynamic cage
- **WHEN** a valid tetrahedral cage is attached with finite compliant root-follow constraints
- **THEN** dynamic points yield under contacts while edge and volume constraints restore their rest configuration

#### Scenario: Internal control point
- **WHEN** a dynamic control is internal and absent from the outer surface faces
- **THEN** it participates in deformation and following without becoming a collision particle against the character's interior core

#### Scenario: Failed simulation step
- **WHEN** projection encounters an inverted tetrahedron, nonfinite state, unsupported interaction or unresolved contact
- **THEN** it reports an explicit failure and preserves the previous complete snapshot, step and revision

### Requirement: Completed rigid-step collision provider
The physics world SHALL expose optional ISoftBodyCollisionQuery functionality returning sphere sweep and penetration contacts for the complete, most recently finished rigid step. Results SHALL carry stable object/feature identity, TOI, outward normal, step-end plane and surface velocity. Queries SHALL respect two-sided collision filters and reject stale step/time input without replacing existing output.

#### Scenario: Moving kinematic obstacle
- **WHEN** a translating kinematic obstacle crosses a softbody point during the completed interval
- **THEN** the query considers its motion during that interval and returns the obstacle surface velocity

#### Scenario: Unsupported contact motion or coupling
- **WHEN** rotating obstacle motion, dynamic rigid reaction, soft-body CCD, edge contacts or self collision is requested without implementation support
- **THEN** the provider or solver returns an explicit unsupported status rather than claiming end-pose overlap is continuous or one-way contact is two-way dynamics

### Requirement: Explicit control modes and shared fixed-step phases
The solver SHALL distinguish dynamic, world-fixed and root-kinematic points. The existing Physics fixed-step owner SHALL sample root input before the rigid update and advance softbody simulation after it. Pause, single-step, reset and interpolation SHALL retain consistent completed-sample semantics. Closing the world SHALL invalidate collision services and revoke pose contribution before borrowed pointers can be used again.

#### Scenario: No rigid contact events
- **WHEN** a rigid step produces no collision events
- **THEN** softbody input and output phases still execute once for that step

#### Scenario: Root-controlled pin
- **WHEN** an author optionally pins a point to the moving root
- **THEN** that zero-inverse-mass point follows its target while dynamic points remain simulated

### Requirement: Compliant XPBD root following
Dynamic root following SHALL use finite positive compliance and participate in the same constraint iteration as edges, volumes and contacts. Multipliers SHALL reset once per fixed step and accumulate across its projection iterations. The implementation SHALL preserve dynamic inverse mass and SHALL NOT substitute a hard pin, auxiliary particle or force spring for this constraint. An empty list SHALL permit free softbody simulation.

#### Scenario: Finite displacement error
- **WHEN** an all-dynamic cage receives a moving root target
- **THEN** its point displacement follows the XPBD compliance/mass relation and can retain a finite target error

#### Scenario: Invalid following constraint
- **WHEN** a constraint identifies a duplicate, out-of-range or non-dynamic vertex, or nonpositive/nonfinite compliance
- **THEN** initialization fails explicitly and preserves the previous valid solver

### Requirement: Mobile runtime boundary
The common softbody runtime SHALL provide a CPU execution path for Android, iOS and OHOS with no required CUDA dependency. Platform support SHALL be reported separately for source compilation, application linking and real-device quality/performance. Desktop tests SHALL NOT count as mobile acceptance. Unsupported nonzero gas pressure SHALL return an explicit error; tetrahedral volume compliance SHALL remain available.

#### Scenario: Backend replacement
- **WHEN** a future Physics backend implements the collision contract
- **THEN** CPU solver, generator, bone mapping and four-weight rendering can be reused without introducing backend types into consumers

### Requirement: Transactional bilateral softbody batch
The existing World adaptor SHALL coordinate its live softbodies in stable actor-identity order through one fixed-step listener. The CPU batch SHALL predict every body, project internal constraints and optional bilateral surface-vertex/triangle contacts with inverse-mass and barycentric weighting, then validate every candidate before committing any body. Closed-volume containment SHALL distinguish interior contacts. Unsupported boundary crossings SHALL fail explicitly. The batch SHALL NOT perform another native rigid-world update or register CPU particles as native rigid objects.

#### Scenario: Two dynamic cages collide
- **WHEN** enabled soft-soft contact brings two supported closed cages together
- **THEN** contact projection distributes opposite corrections between both bodies while preserving fixed and root-kinematic inverse-mass semantics

#### Scenario: One member cannot finish
- **WHEN** any batch candidate fails validation or a publication contract is stale
- **THEN** no member publishes a newer completed snapshot and the World reports the failed attempt explicitly

#### Scenario: Small residual at simultaneous shell and ground contact
- **WHEN** the original projection leaves a small correctable shell residual and the complete batch is eligible for bounded contact refinement
- **THEN** the solver may project the remaining contacts while preserving the requested number of internal constraint iterations
- **AND** it commits only after the original soft, rigid, geometry and crossing validations pass for every body

### Requirement: Optional owned step diagnostics
The Physics softbody interface SHALL expose per-World configuration and owned step diagnostics without exposing implementation types. Profiling SHALL be disabled by default and read no solver clocks while disabled. Enabled diagnostics SHALL retain bounded fixed-step history, operation/contact counts, committed/failure status and non-overlapping elapsed-time buckets. Expired history and closed sessions SHALL return explicit failures without replacing caller output.

#### Scenario: Several fixed steps in one application loop
- **WHEN** the application catches up multiple fixed steps before the consumer reads diagnostics
- **THEN** the consumer can obtain every retained completed or failed attempt rather than only the last step

### Requirement: Reproducible collision workload measurement
External performance fixtures SHALL distinguish actual soft-soft contact and vertical-wall contact from internal-only work using matched authored motion and explicit collision flags. Reports SHALL retain requested and observed topology, mass/follow normalization, proxy quality, fixed-step configuration, package/device provenance, complete solver elapsed time, collision buckets and native rigid cost separately. Failed or noncontact runs SHALL NOT be reported as successful zero-cost contact results. Measurements of a changed wall fixture SHALL NOT be described as identical-scene comparisons with earlier ground-contact data.

#### Scenario: Compare the five rabbit rigs
- **WHEN** the 30/25/20/15/10-control original-rabbit rigs are measured in single/pair workloads and then ten-body workloads
- **THEN** each baseline/contact pair uses the same authored input path, actual contacts are verified, and successful contacting-step distributions and peaks are reported alongside explicit failure/N/A outcomes and geometry differences

### Requirement: Live softbody collision visualization
The Physics world control SHALL expose an owned snapshot of each active body's published particle positions, committed fixed-step positions, distance edges, collision surface triangles and surface particle radius. The Android rabbit demo SHALL offer a touch switch to display the published particles, constraints, collision geometry and the active rigid shapes using the scene camera.

#### Scenario: Inspect a moving rabbit
- **WHEN** the user enables collision visualization during an ordinary run
- **THEN** the overlay follows the same interpolated particle positions used by skeletal animation and identifies the rigid ground geometry separately

#### Scenario: Return to the ordinary scene
- **WHEN** the user turns visualization off
- **THEN** the overlay clears, debug sampling stops and the collision simulation continues in the same session

### Requirement: Inspect and tune live soft bodies
The Physics softbody interface SHALL report each active body's control count, solver iterations, material damping, pressure and actual ranges of constraint compliance. A host SHALL be able to patch the compliance of existing constraints and the damping of named simulation instances together between steps. The change SHALL preserve particle positions, velocities, rest geometry and the persistent rig asset. Invalid selections or values SHALL leave all selected instances unchanged.

#### Scenario: Compare tuning on the phone
- **WHEN** the user changes a material parameter on the rabbit HUD
- **THEN** the two active rabbits receive the selected value and the HUD displays the solver readback
- **AND** restoring the values reloads the active rig's original tuning without rebuilding either body

#### Scenario: Switch the control count
- **WHEN** the user selects a 10, 15, 20, 25 or 30 point rig in the ordinary Android scene
- **THEN** both rabbits change to the matching rig, skeleton, mesh and skin together at a grounded separation
- **AND** the HUD reports the actual control count for each rabbit in the same running scene

### Requirement: Contact-driven rabbit demonstration
The ordinary two-rabbit demonstration SHALL release its inward drive when the rear supports load and update both root velocities from the same depth and closing velocity. The visible response SHALL use the simulated and skinned control cage. The demonstration SHALL support a repeated approach and a grounded jump or rig switch after separation.

#### Scenario: Compare the default pair with the original XPBD rabbit
- **WHEN** two default 15-point rabbits approach back-to-back at 1.20 m/s per root
- **THEN** validation records actual bilateral soft contact, the relative opening speed and the collision-axis shortening of the skinned trunk
- **AND** it compares the measurements and visible collision phases against the original XPBD soft-bounce pair with compression 1 and rebound 0.5
