## ADDED Requirements

Delivery status (2026-10-27): explicit cell lifecycle, snapshot-based preparation, existing constraint/support pins, inline manifest persistence and benchmarks are implemented. Spatial selection/coverage policy, extended manifests, stable cross-cell dependency resolution, Terrain pager integration and expanded performance/platform validation remain subsequent work. See remaining-work.md and tasks 6.5a–6.8a; the requirements below describe the complete intended contract, not a claim that every scenario has shipped.

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
Requests SHALL retain immutable product revisions and world-session/owner generations. A stale, cancelled or retired request SHALL NOT mutate an active world. Native shapes, jobs and callbacks SHALL retain the required SDK and module leases. Shutdown SHALL prevent new publication, collect outstanding work and retirement, and release leases only after their last user.

#### Scenario: Unload and immediately reload a cell
- **WHEN** a previous generation completes after the cell has been unloaded and requested again
- **THEN** the old result is discarded and cannot create duplicate bodies or overwrite the current request

#### Scenario: World destroyed while preparation runs
- **WHEN** the owning world ends before an asynchronous preparation completes
- **THEN** completion only releases its result, never accesses the destroyed world, and outstanding code/SDK dependencies remain valid until work is collected

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
