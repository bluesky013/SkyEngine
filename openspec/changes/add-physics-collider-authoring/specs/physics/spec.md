### Scope note: `physics-collider-authoring` / ADDED

Delivery status (2026-10-27): version-1 geometry/material codecs, stable element IDs, runtime references, local rotation, independent builder publication and reflected component fields are implemented. Recipe/dependency fingerprints, full transform validation, generation/import/decomposition workflows and the dedicated authoring editor/preview remain subsequent work R1–R3/R7 in remaining-work.md. The full requirements below must not be interpreted as completed editor/tool delivery.

### Scope note: `physics-streaming` / ADDED

Delivery status (2026-10-27): explicit cell lifecycle, snapshot-based preparation, existing constraint/support pins, inline manifest persistence and benchmarks are implemented. Spatial selection/coverage policy, extended manifests, stable cross-cell dependency resolution, Terrain pager integration and expanded performance/platform validation remain subsequent work. See remaining-work.md and tasks 6.5a–6.8a; the requirements below describe the complete intended contract, not a claim that every scenario has shipped.

## ADDED Requirements

### Requirement: Reusable collision sources and independent products

The system SHALL store editable collision recipes and stable shape identities in a plugin-owned source asset, and SHALL publish versioned geometry products consumable without render source files. Body kind, mass and filters SHALL remain instance settings.

#### Scenario: Shared geometry with independent bodies
- **WHEN** two bodies reference one Collision asset with different mass and filters
- **THEN** they share immutable geometry while retaining their independent body settings

#### Scenario: Stale product cannot silently ship
- **WHEN** source save succeeds but product cooking fails
- **THEN** the editor preserves the last successful product, marks it stale, and packaging rejects that stale product

### Requirement: Persistent body material references

Physics materials SHALL have plugin-owned versioned source/product data and stable asset UUIDs. Body authoring data SHALL reference materials by UUID, independently of shared collision geometry. The initial asset schema SHALL expose one friction value, restitution and combine rules; damping SHALL be an explicit body setting. Runtime material handles SHALL be resolved and owned per physics world, never serialized into assets or scenes.

#### Scenario: Shared material across two worlds
- **WHEN** two worlds load bodies referencing the same material asset
- **THEN** they may share immutable product values but each resolves its own runtime material handle and unloading one world does not invalidate the other

#### Scenario: Default versus missing material
- **WHEN** a legacy body has no material UUID or a new body explicitly selects the default
- **THEN** it uses the documented default material
- **WHEN** a nonempty material UUID cannot resolve to a compatible product
- **THEN** the body remains unactivated with an explicit error rather than silently using the default

#### Scenario: Package a material-only dependency
- **WHEN** a material is referenced only by a PhysicsBody in a scene cell
- **THEN** the cell dependency list and package audit include its typed UUID even without a rendering reference

### Requirement: Explicit geometry generation and limits

The authoring system SHALL provide box, sphere, capsule, single convex hull, compound and static triangle mesh workflows with visible inputs, effective parameters, validation results and budgets. It MUST reject dynamic or kinematic triangle meshes and triangle-mesh volume triggers under the initial runtime contract.

#### Scenario: Concave dynamic prop
- **WHEN** the user assigns a concave triangle mesh to a dynamic body
- **THEN** validation rejects the assignment and offers primitive/convex composition without silently changing the body kind

#### Scenario: Degenerate fitting input
- **WHEN** fitting receives non-finite points, invalid indices or dimensions below the supported minimum
- **THEN** it returns a localized error and preserves the previous authored shapes

### Requirement: Shape transforms and legacy compatibility

The system SHALL support local shape rotation and translation through nested compounds with a documented transform order. Existing inline serialized shapes without rotation or asset-reference fields SHALL retain identity rotation and Inline source mode.

#### Scenario: Legacy component load
- **WHEN** an existing PhysicsBody document without new fields is opened
- **THEN** its shape and serialized enum meanings remain unchanged

#### Scenario: Invalid asset reference
- **WHEN** Asset mode references a missing or incompatible Collision product
- **THEN** the system returns an explicit error without substituting an inline default shape

### Requirement: Reflected and undoable authoring UI

The editor SHALL use the shared asset-document, reflected-form, command and action registries. A drag or accepted generation result SHALL be one undoable operation, and cancellation SHALL restore the starting state.

#### Scenario: Cancelled shape drag
- **WHEN** the user presses Escape while resizing a capsule
- **THEN** the original parameters and preview are restored without a committed undo entry

#### Scenario: Generated group replacement
- **WHEN** a valid generated group is applied then undone
- **THEN** the previous shapes, identities and selection are restored through the document command service

### Requirement: Generation tasks and reimport are transactional

Generation SHALL use document revisions, bounded work and exactly one completion notification. Reimport SHALL preserve manual shapes and expose conflicts for modified generated elements. Decomposition that exceeds its budget MUST report unmet quality rather than silently truncate output.

#### Scenario: Stale background result
- **WHEN** generation completes after the document is edited, cancelled or closed
- **THEN** the result is discarded without changing the current document and all task resources are released

#### Scenario: Reimport with edited generated shapes
- **WHEN** the source mesh changes after a generated shape was manually edited
- **THEN** the editor reports a merge conflict while retaining that edit and unrelated manual shapes

### Requirement: Physics preview and replacement preserve world ownership

Preview SHALL use an isolated physics world with reset, pause and single-step. Applying new geometry SHALL validate and construct it before replacing live state, preserve or explicitly rebind dependent constraints, and retain the old state on failure.

#### Scenario: Failed live shape replacement
- **WHEN** candidate geometry cannot be cooked or installed
- **THEN** the original body and its constraints remain usable and the failure is reported

### Requirement: Offline dependencies and unique metadata owners

Runtime adaptor and render modules MUST NOT depend on editor or builder targets. Builder MUST NOT depend on runtime adaptor or render implementations. Shared source codecs SHALL perform no reflection registration; each reflected type SHALL have one definition owner, and its defining DLL code SHALL remain process resident without per-object code tokens.

#### Scenario: Runtime-only deployment
- **WHEN** the game is packaged without authoring tools and CoACD
- **THEN** Collision products load and simulate through the Jolt adaptor without those tool dependencies

#### Scenario: Editor module restart with open work
- **WHEN** an authoring module shuts down with an open document or task
- **THEN** tasks are cancelled and collected, registrations are removed symmetrically, and callback captures and required SDK/data owners remain valid until their last use; DLL code remains resident after release

### Requirement: Separate asset preparation from physics activation

The physics integration SHALL expose asynchronous preparation of immutable collision geometry and material products, a nonblocking readiness/error result, and explicit instance activation and retirement. Runtime requests SHALL use existing asset tasks with ProductsOnly semantics and MUST NOT synchronously wait for loading or perform source-asset cooking in World tick, attachment, query or contact callbacks. Asset completion alone SHALL NOT mean that collision is active.

#### Scenario: Prepared geometry is not query-visible
- **WHEN** products and native shapes are Ready but their instance batch has not been committed
- **THEN** no body from that batch participates in queries, contacts or simulation

#### Scenario: Product is unavailable
- **WHEN** a runtime request cannot resolve a required collision or material product
- **THEN** it reports a failed dependency without source cooking, a default collider or successful region activation

### Requirement: Immutable shared preparation and bounded residency

Compatible backend sessions SHALL share immutable prepared geometry by product content, local geometry transform, effective scale, collision settings and SDK/build identity. Mass, body pose, filters and body-level material SHALL NOT create distinct geometry entries. Identical in-flight requests SHALL share preparation while retaining independently releasable consumer references. Active instances and tasks SHALL pin required resources; unused cache entries SHALL be evictable under explicit memory and concurrency budgets.

#### Scenario: Unload one of two consumers
- **WHEN** two cells share a prepared shape and material product and one cell retires
- **THEN** the remaining cell keeps valid resources and its world-owned body/material state

#### Scenario: Cancel only one request
- **WHEN** a consumer cancels a shared in-flight preparation request
- **THEN** its result cannot activate an instance and other consumers continue to receive their result

#### Scenario: Repeated boundary crossing
- **WHEN** cells are repeatedly loaded and retired across a streaming boundary
- **THEN** temporary preparation, pending requests and unused CPU/native cache storage remain within configured bounds, with backpressure when required

### Requirement: Safe-point bounded activation and retirement

Physics instance batches SHALL be committed or retired only at a host-controlled safe point after prior simulation jobs and event dispatch have completed and before the next simulation/query phase. Activation SHALL preflight capacity and prepare candidates without broadphase publication. An atomic activation group SHALL be bounded and exposed completely or not at all; oversized groups SHALL be split by the cook/host workflow or rejected. Retirement SHALL resolve constraint and persistent-object ownership before removing bodies and releasing resources.

#### Scenario: Capacity failure during activation
- **WHEN** an activation group cannot allocate all required instances
- **THEN** only new candidates are rolled back, the group is not query-visible, existing active regions remain intact and a failure is returned

#### Scenario: Region retirement ends contacts
- **WHEN** a region is retired after the previous physics step
- **THEN** its contacts end according to the existing ordered event contract without callbacks dereferencing destroyed Actors, and its resource references are released after removal

### Requirement: Generation-checked completion and shutdown

Requests SHALL retain immutable product revisions and world-session/owner generations. A stale, cancelled or retired request SHALL NOT mutate an active world. Native shapes, jobs and callbacks SHALL retain the required SDK state, inputs and runtime objects; defining DLL code SHALL remain process resident without per-object code tokens. Shutdown SHALL prevent new publication, collect outstanding work and retirement, and release owned state only after its last user.

#### Scenario: Unload and immediately reload a cell
- **WHEN** a previous generation completes after the cell has been unloaded and requested again
- **THEN** the old result is discarded and cannot create duplicate bodies or overwrite the current request

#### Scenario: World destroyed while preparation runs
- **WHEN** the owning world ends before an asynchronous preparation completes
- **THEN** completion only releases its result, never accesses the destroyed world, and actual SDK state and object owners remain valid until work is collected, while defining DLL code stays process resident

#### Scenario: Product replacement fails
- **WHEN** a new geometry/material revision cannot prepare or commit
- **THEN** current instances retain their old compatible revisions, handles and dependent constraints; successful replacement preserves the documented body state and does not mutate shared shapes in place

### Requirement: World-owned streaming with explicit collision coverage

Spatial selection, priorities, cell identity and ownership SHALL belong to the World host rather than a Jolt-specific partition manager. All interacting cells in a game World SHALL use the same physics world. The host SHALL distinguish required collision coverage from render visibility and distinguish Ready products from Active coverage. Collision requests SHALL support prefetch and unload hysteresis and consider movement, destination and support dependencies.

#### Scenario: Teleport destination is still preparing
- **WHEN** a teleport or spawn requires a region whose collision is not Active
- **THEN** readiness remains pending or failed and the host defers entry or applies an explicitly configured conservative policy; it does not interpret a raycast miss as proof that the region has no geometry

#### Scenario: Render tile leaves visibility
- **WHEN** a render LOD or visibility decision removes a tile still required for physics support
- **THEN** that decision alone does not retire its collision; the host retains the required collision product until its physics residency dependency is released

### Requirement: Dynamic objects and cross-cell relationships survive residency changes

Persistent moving bodies SHALL be owned by stable World instances independently of their source cell. Retiring a source cell SHALL neither destroy a promoted body nor duplicate it on reload. Constraint endpoints SHALL resolve through stable instance identities and remain resident together while required. An unresolved endpoint MUST NOT be treated as a world anchor; dependency groups and pins SHALL have explicit budgets and failure states.

#### Scenario: Body moves out of its source cell
- **WHEN** a dynamic body moves into another active region and its source cell unloads
- **THEN** the same live body remains owned by the World with its velocity and constraints, and reloading the source cell does not spawn a duplicate

#### Scenario: Unload a constrained endpoint
- **WHEN** a cell requested for retirement contains an endpoint required by a still-active constraint
- **THEN** retirement is deferred by a residency pin or handled by an explicit joint/object migration policy, without silently deleting the constraint

### Requirement: Persistent descriptions and package discovery are independent of residency

The host persistence contract SHALL represent World configuration, stable cell and actor/component identities, cell bounds, product locations and explicit dependencies. Cells SHALL persist body parameters and collision/material asset UUIDs without native handles. Runtime loading SHALL resolve only requested cell dependencies; offline package validation SHALL traverse the entire declared cell set. Saving a partially resident world SHALL preserve unmodified unloaded cells. Current handle-based PhysicsWorldState SHALL NOT be used as a cross-session scene/save format.

#### Scenario: Save with unloaded cells
- **WHEN** an editor saves a world with only some cells resident
- **THEN** unloaded cell records and their references remain in the document and are not deleted by enumerating live Actors

#### Scenario: Package a cell never opened in the editor
- **WHEN** an unvisited cell references collision and material products
- **THEN** packaging checks their full required product closure without eagerly loading every cell at game startup

#### Scenario: Restore persistent runtime state
- **WHEN** a host implements cross-session state restoration for a streamed object
- **THEN** it resolves stable instance identity after loading descriptors/products and recreating runtime handles, rather than interpreting a previous session's BodyID or material handle as persistent identity

#### Scenario: Legacy world load
- **WHEN** a world uses the existing unpartitioned inline scene format
- **THEN** it remains loadable as a complete world without inventing cell files or repeating global World::Build for later cell activation

### Requirement: Reproducible streaming benchmark cases

The physics tests SHALL provide explicitly selected benchmarks using the existing product and World streaming paths. They SHALL cover shared geometry/material products, independent shapes, resident resource reuse, triangle meshes and admitted-preparation cancellation followed by reload. Results SHALL identify workload size, build mode, phase sample counts, wall and host-Tick timing distributions, polling/drain overhead and the scope of sampled residency counters. Fixture generation/cook SHALL be reported separately from runtime phases. Tests SHALL assert lifecycle correctness and resource cleanup, without machine-specific latency pass/fail thresholds.

#### Scenario: Repeated paging benchmark
- **WHEN** a benchmark repeatedly prepares, activates and retires a group of cells
- **THEN** it verifies that Ready cells are not query-visible, Active cells have the expected bodies and retired groups release their resources except explicitly retained anchors
- **AND** it records preparation, activation and retirement measurements separately

#### Scenario: Ordinary regression run
- **WHEN** the regular physics test suite runs without explicitly enabling benchmarks
- **THEN** heavier benchmark workloads SHALL remain disabled while the existing correctness tests run normally

#### Scenario: Interpretation of memory and first-load results
- **WHEN** the benchmark reports source-byte residency or first-load latency
- **THEN** the report SHALL NOT describe those values as total native/process memory or a controlled OS-cold disk measurement

### Requirement: Spatial host policy acceptance

The subsequent Framework/game-host policy SHALL use explicit collision demand, stable cell bounds/product references and dependency groups to schedule readiness and activation. It SHALL implement prefetch/unload hysteresis and bounded priority processing without a separate Jolt partition manager. Teleport/spawn and high-speed entry SHALL consult required Active coverage and report Pending/Failed explicitly. Pin exhaustion and unresolved stable endpoints SHALL have explicit outcomes rather than unbounded residency or implicit world anchors.

#### Scenario: Required region fails to prepare
- **WHEN** a destination or predicted movement region cannot obtain valid physics products
- **THEN** the host SHALL defer the dependent entry or use an explicitly configured conservative policy and SHALL preserve still-required active collision

#### Scenario: Coverage dependencies exceed the configured budget
- **WHEN** dependency pins or a residency group exceed the declared host budget
- **THEN** the host SHALL return observable backpressure/failure without silently deleting support or leaving a partial dependency group active

### Requirement: Terrain paging acknowledges physics residency

The subsequent Terrain bridge SHALL participate in physics preparation, activation and retirement acknowledgements. Tile eviction SHALL retain required product/geometry resources until active support and constraint dependencies permit safe retirement. A render tile's LOD/visibility lifecycle SHALL NOT be treated as physics eviction permission.

#### Scenario: Supported object outlives a render tile
- **WHEN** Terrain paging requests removal of a tile whose collision still supports an active moving body
- **THEN** the collision and its dependencies SHALL remain resident until the support dependency is released or an explicitly validated replacement becomes Active

### Requirement: Streaming performance investigation and platform validation

Subsequent performance work SHALL profile observed host-Tick spikes before assigning a root cause or changing budgets. It SHALL report actor preparation, product loading, native Shape work and commit costs separately, with declared hardware/build/workload and optimized-build baselines. Sustained tests SHALL measure native/process memory as well as current source-byte/task counters, verify bounded growth and release, and exercise configured host budgets. Shipping validation SHALL consume products without source files or authoring modules and SHALL enumerate platform results and untested cases explicitly.

#### Scenario: Debug benchmark shows a preparation spike
- **WHEN** a repeatable preparation-Tick spike is observed in shared or already-resident Shape workloads
- **THEN** profiling evidence SHALL identify the contributing host/worker stages before an optimization is attributed to Jolt cooking or asset IO
- **AND** the existing lifecycle and cancellation regressions SHALL remain passing after any resulting change

#### Scenario: Long-running paging validation
- **WHEN** sustained paging, cancellation and destination changes run on a declared supported configuration
- **THEN** the report SHALL include native/process-memory trends, pending work, post-retirement resource counts and host-budget results rather than substituting accounted geometry bytes for total memory
