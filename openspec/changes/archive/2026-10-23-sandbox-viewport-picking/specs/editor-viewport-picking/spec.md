Scope: stages A and B of `sandbox-viewport-picking`. Phase A evidence does not imply the new phase B requirements are implemented. The GPU backend described as phase C in the design is outside this delta's implementation scope.

## ADDED Requirements

### Requirement: Shared single Actor selection by viewport click
The editable viewport SHALL select the owning Actor of the resolved eligible mesh or editor-proxy hit on a left click and SHALL clear selection on a complete miss, using SelectionService as its sole selection owner. World-surface selection SHALL resolve the nearest eligible surface subject to the declared occlusion and overlay policy. Selection SHALL update the Outliner, Inspector and Gizmo without changing transforms, World dirty state or command history. Q and transform tools SHALL share this behavior outside their handles. Component, part and subelement identity SHALL NOT introduce independent subelement selection in this change.

#### Scenario: Select a mesh without a collider
- **WHEN** an eligible click hits a supported static mesh Actor that has no physics collider
- **THEN** SelectionService SHALL select that Actor and all selection consumers SHALL reflect it

#### Scenario: Click empty scene content
- **WHEN** a complete supported query returns no hit for an eligible click inside the scene content
- **THEN** the current selection SHALL be cleared without a World command or dirty state change

### Requirement: Precise geometric nearest hit
GeometrySurface picking SHALL use bounds only as a broad phase and SHALL confirm static triangle intersections before returning a hit. It SHALL honor the supported visibility, clipping and culling semantics, compare distances in world space and support parent transforms, invertible non-uniform scales and negative scales. Invalid geometry or singular transforms SHALL have explicit outcomes. Unsupported material or deformation semantics SHALL NOT be presented as pixel-accurate results. EditorProxy hits SHALL be explicitly identified and SHALL NOT fabricate triangle-surface position or normal data.

#### Scenario: Bounds contain empty space
- **WHEN** a ray intersects a mesh bounds but none of its triangles
- **THEN** that bounds intersection SHALL NOT select the Actor or hide a valid farther triangle hit

#### Scenario: Overlapping transformed instances
- **WHEN** parented instances with different non-uniform or negative scales overlap under the ray
- **THEN** the closest eligible world-space triangle intersection SHALL win regardless of local ray parameter magnitude

#### Scenario: Invalid geometry
- **WHEN** a relevant candidate has non-finite vertices, out-of-range indices or a non-invertible transform
- **THEN** the query SHALL return an explicit diagnostic without dereferencing invalid storage or modifying selection as if it were a complete miss

### Requirement: Gesture priority and click threshold
The viewport SHALL recognize a scene click only after matching pointer/button release within its content rectangle, with no intervening movement beyond the themed threshold or context cancellation. Modal/menu input, active captures, navigation and Gizmo handles SHALL take priority. Moving past the threshold SHALL disqualify the click even after returning to the start.

#### Scenario: Manipulation does not change selection
- **WHEN** a Gizmo drag, Alt+LMB orbit, RMB look or MMB pan ends over another Actor
- **THEN** no scene click query SHALL be issued and selection SHALL remain owned by the existing interaction

#### Scenario: Drag outside and return
- **WHEN** a left press moves beyond the threshold or leaves the content before releasing outside
- **THEN** it SHALL NOT be interpreted as a click, including when an over-threshold drag later returns near the starting point

### Requirement: Consistent surfaces and owned versioned queries
Queries SHALL derive coordinates from the actual scene image content using the same camera and DPI mapping as the viewport. Query data and geometry SHALL remain owned and immutable until completion. Results SHALL carry context identity and SHALL be applied only if document, scene, camera, layout, surface, request sequence and target identity remain valid. Only the host thread SHALL mutate SelectionService.

#### Scenario: Floating viewport at different DPI
- **WHEN** the viewport moves to a floating surface with a different scale
- **THEN** a new click SHALL generate the corresponding scene ray without a second DPI conversion and any old candidate/result SHALL be invalidated

#### Scenario: Delayed result after context change
- **WHEN** a document switch, object deletion, camera movement, asset revision, Play transition or newer click invalidates an in-flight request
- **THEN** its completion SHALL NOT overwrite the current selection or access released actor/component storage

### Requirement: Explicit readiness and failure semantics
The picker SHALL distinguish Hit, Miss, Pending, Unsupported and Error. Pending, Unsupported and Error SHALL preserve selection and expose status feedback. A potentially nearer unresolved candidate SHALL prevent falsely declaring a complete miss or a farther nearest hit. Requests SHALL NOT block pointer dispatch on disk loading, BVH preparation or GPU waits.

#### Scenario: Relevant geometry is still loading
- **WHEN** a candidate intersecting the query range lacks ready CPU geometry
- **THEN** the result SHALL be Pending, selection SHALL remain unchanged and only the latest still-valid request SHALL be eligible for retry

#### Scenario: Unsupported surface semantics
- **WHEN** a potentially selected candidate requires unsupported deformation or material behavior
- **THEN** the result SHALL report Unsupported instead of silently substituting a bounding-box hit or clearing selection

### Requirement: Bounded reusable geometry cache and module boundaries
Geometry acceleration SHALL be shared across instances by asset identity and geometry revision, bounded by an explicit memory budget and invalidated on changed geometry. EditorCore's picker contract SHALL contain no UI/Aurora/RHI dependencies; concrete adapters SHALL remain in existing editor targets. Aurora SHALL NOT depend on Sandbox interfaces, and runtime selection SHALL NOT require editor modules.

#### Scenario: Geometry reload and instance movement
- **WHEN** an instance transform changes without a geometry change, or an asset reload creates a new geometry revision
- **THEN** the former SHALL reuse the immutable mesh acceleration and the latter SHALL invalidate the old revision without applying stale query results

#### Scenario: Cache exceeds its budget
- **WHEN** prepared geometry would exceed the configured cache budget
- **THEN** unused entries SHALL be evicted or preparation SHALL report a recoverable status, while in-flight owned snapshots SHALL remain valid

### Requirement: Explicit request policy and versioned target identity
Requests SHALL contain an owned view snapshot and viewport-local logical/image-pixel coordinates derived from the same content rectangle, actual image extent and camera. They SHALL identify selection policy and filter revision. Results SHALL carry stable Actor/component/instance/provider identity with generations and distinguish GeometrySurface, EditorProxy and VisibleSurface semantics. Optional surface data SHALL only be populated when actually computed under that semantic contract.

#### Scenario: Image scale differs from window DPI
- **WHEN** a floating viewport uses a different DPI or its scene image resolution differs from its logical content size
- **THEN** the image pixel and ray SHALL use the same view snapshot and one explicit mapping, without applying DPI twice or clamping an outside click onto an image edge

#### Scenario: Visible-surface policy without a GPU backend
- **WHEN** a caller requests VisibleSurface from the phase B implementation
- **THEN** the request SHALL return Unsupported and SHALL NOT silently downgrade to a CPU geometry or bounds result

#### Scenario: An instance or provider identifier is reused
- **WHEN** a deleted instance or unregistered provider is replaced with the same local identifier but a different generation
- **THEN** an older result SHALL NOT resolve to the replacement object

### Requirement: Selectability and occlusion remain distinct
The picker SHALL distinguish selectable surfaces from occluders. A visible locked or selection-filtered surface SHALL remain an occluder unless explicitly ignored for occlusion. Hidden surfaces SHALL be excluded according to the view visibility contract. Depth-tested proxies SHALL respect supported scene occlusion; only explicitly overlaid proxies whose visuals use the same policy MAY precede world surfaces. Resolution SHALL be stable for equal-depth candidates and independent of provider or tree traversal order.

#### Scenario: A locked front mesh hides a selectable mesh
- **WHEN** the ray first reaches a supported visible occluding mesh that is locked or filtered from selection
- **THEN** the picker SHALL NOT select a farther mesh through it and MAY report a complete Miss with an Occluded reason once completeness is established

#### Scenario: An overlay icon and a world surface overlap
- **WHEN** a visible editor icon declares Overlay in both its visual and pick data
- **THEN** the resolver SHALL apply the declared overlay priority and return EditorProxy semantics if that icon wins

#### Scenario: Equal-depth provider hits
- **WHEN** two eligible world-surface or depth-tested candidates are within the defined nearest-distance tolerance band
- **THEN** priority, screen error and a stable target key SHALL resolve the tie consistently without a non-transitive tolerance-based sort

#### Scenario: Overlapping overlay visuals
- **WHEN** visible overlay proxies overlap
- **THEN** drawing and picking SHALL use the same priority and stable target ordering; the winner SHALL be painted last, independently of pointer distance, world depth and provider registration order

#### Scenario: An unresolved candidate has no conservative bounds
- **WHEN** a relevant scene entry cannot supply conservative bounds
- **THEN** it SHALL remain represented as an unresolved candidate and SHALL NOT disappear merely because it cannot be inserted into the spatial index

### Requirement: Producer-declared surface capabilities
CPU eligibility SHALL derive from versioned surface facts supplied by the resource/program producer, including position preservation, topology/layout, coverage, culling and depth behavior. The picker SHALL NOT infer support from a material UUID, shader name or unverified bytecode inspection. Unknown or unsupported facts SHALL preserve selection and report Unsupported. An authored static opaque material with an explicitly supported contract SHALL be eligible without requiring a built-in material identity.

#### Scenario: Authored material declares supported static geometry
- **WHEN** a producer supplies a renderable authored material with a verified static opaque geometry contract
- **THEN** it SHALL participate in GeometrySurface picking using the shared geometry BVH, irrespective of its material UUID

#### Scenario: Coverage or deformation capability changes
- **WHEN** a material/program revision changes from supported static coverage to unknown, alpha-test or deformed coverage
- **THEN** its capability revision and related pending results SHALL be invalidated without rebuilding unchanged mesh geometry or continuing to claim the previous capability

### Requirement: Scoped extensible editor pick providers
EditorCore SHALL define the consumer-facing provider and owned proxy data contracts. Existing editor implementations SHALL register providers through a scoped host with explicit registration generation, status and removal. Providers SHALL publish triangle, world-segment or billboard-rectangle proxies with target, revisions, bounds/layout and depth policy. Collection SHALL occur during bounded host preparation, not during pointer dispatch. Workers SHALL NOT invoke plugin provider callbacks or mutate selection.

#### Scenario: A non-mesh editor object is selected
- **WHEN** a registered provider supplies a visible line or icon proxy for an Actor without mesh or collider
- **THEN** an eligible click SHALL resolve EditorProxy semantics and select that Actor through the existing SelectionService

#### Scenario: Fixed-size screen proxy after camera movement
- **WHEN** camera, DPI or layout changes the projection of a fixed-pixel-size icon
- **THEN** its hit layout SHALL be refreshed consistently with its visual layout and SHALL NOT be falsely culled by a stale world-space bound

#### Scenario: Provider unregisters with outstanding work
- **WHEN** a provider is unregistered while its snapshots or requests remain in flight
- **THEN** new collection SHALL stop, its generation SHALL invalidate dependent results, and required data and module leases SHALL remain alive until work and destructors are safely retired

#### Scenario: Transform Gizmo consumes a click
- **WHEN** an existing IGizmoVisualProvider handle captures manipulation
- **THEN** the scene provider system SHALL NOT issue an Actor-selection click or replace the existing handle drawing/dragging contract

### Requirement: Two-level acceleration and revision-driven maintenance
World queries SHALL use an instance spatial index before shared local mesh BVHs. Stable instance entries SHALL cache inverse transforms and conservative world bounds. Geometry, transform, capability, topology and readiness changes SHALL be distinguishable. Unchanged and camera-only world snapshots SHALL NOT rebuild or update world acceleration. Parent transform changes SHALL update affected descendants. Scene data SHALL be obtained from the existing extraction flow rather than a second component traversal.

#### Scenario: Sparse instance movement
- **WHEN** a small subset of instances moves while mesh geometry remains unchanged
- **THEN** only affected instance transforms and bounds SHALL be recomputed, ancestor bounds SHALL be maintained, and shared mesh build counts SHALL remain unchanged

#### Scenario: Budgeted index maintenance overlaps scene edits
- **WHEN** an instance is inserted, moved onto the ray or deleted during suspended index maintenance
- **THEN** queries SHALL remain incomplete until the new revision is indexed, restart obsolete traversal cursors and SHALL NOT return a false Miss or hit a deleted/stale leaf; lost delta continuity SHALL trigger owned full resynchronization

#### Scenario: Delta continuity is lost
- **WHEN** an extraction delta does not match the index base revision
- **THEN** the picker SHALL resynchronize from owned scene data and SHALL keep affected queries incomplete until valid indexing is available

#### Scenario: Readiness advances without a semantic change
- **WHEN** a previously requested mesh or acceleration structure becomes ready with unchanged content identity
- **THEN** readiness SHALL wake an otherwise valid pending request without making that request stale through an artificial scene semantic change

### Requirement: Bounded non-blocking request and preparation lifecycle
The picker SHALL provide explicit non-blocking submission, polling and cancellation with bounded per-viewport and host capacity. Duplicate identical request tokens SHALL be idempotent. Host index maintenance and traversal SHALL support budgeted continuation; exhausted budgets SHALL NOT be interpreted as complete results. Geometry work SHALL be coalesced by immutable geometry identity, cancellable and published only after successful completion. Memory accounting SHALL include active, preparing, retiring and scratch allocations and worker-retained payload ownership.

#### Scenario: A newer click replaces an old pending click
- **WHEN** the same viewport submits a newer request before an older request completes
- **THEN** only the newest valid request SHALL remain eligible to update selection and stale/cancelled work SHALL NOT emit a synthetic Miss

#### Scenario: Query traversal exceeds its host budget
- **WHEN** a deeply overlapping scene cannot be resolved within the configured host work budget
- **THEN** traversal SHALL continue through bounded state as Pending or fail with an explicit resource limit, while pointer dispatch remains independent

#### Scenario: Readiness or index publication changes a suspended traversal
- **WHEN** an index update or newly ready candidate changes the indexed data between traversal slices
- **THEN** the continuation SHALL restart against the corresponding generation or safely reject its old snapshot result, without retaining invalid node references or skipping a newly available nearer hit

#### Scenario: One consumer cancels shared geometry preparation
- **WHEN** two valid viewports require the same geometry and one cancels
- **THEN** the remaining consumer SHALL retain preparation ownership, and no partial or cancelled BVH SHALL be published

#### Scenario: All apparent cache entries are evicted but work retains bytes
- **WHEN** an in-flight worker or retiring snapshot still owns an evicted allocation
- **THEN** the bytes SHALL remain counted against the relevant resident/preparing/retiring budget until actually released

### Requirement: Measured correctness and scaling evidence
Verification SHALL include an exhaustive-query oracle, degenerate and transformed inputs, deterministic ties, provider lifetimes and dirty/update counters. Performance evidence SHALL separate decoding, building, querying, scheduling and click-to-selection latency; report distributions, workload sizes and memory high-water marks; and identify configuration and hardware. The phase A measurements SHALL NOT be presented as phase B or GPU verification.

#### Scenario: Scaling claims are evaluated
- **WHEN** the implementation is evaluated on the 200-instance baseline and larger sparse or overlapping scenes
- **THEN** reports SHALL include actual candidate/node counts and p50/p95/max latency, explicitly distinguish design targets from measured outcomes, and preserve existing Vulkan/DX12 acceptance coverage

### Requirement: Provider-owned geometry queries
EditorCore SHALL expose a custom geometry query interface through the existing scoped provider snapshot. Entries SHALL bind an owning Actor, stable generation, optional conservative world bounds and immutable query ownership. Providers MAY retain their own BVH or spatial index. The host SHALL apply visibility, selection and occlusion filters, perform bounded bounds screening, and compose exact world-space hits with the existing mesh/proxy resolver. VisibleSurface SHALL remain unsupported by this CPU extension.

#### Scenario: Custom geometry participates without a mesh payload
- **WHEN** a provider returns a supported exact surface hit from its own prepared query object
- **THEN** the picker SHALL validate distance, optional surface data and entry identity, resolve GeometrySurface semantics, and select only the owning Actor without copying or rebuilding the provider's spatial index

#### Scenario: A custom query yields and is cancelled
- **WHEN** a query exhausts its cooperative work/time budget or its request is cancelled, replaced, invalidated or unregistered
- **THEN** Pending SHALL retain a provider-owned continuation only while valid; cancellation SHALL destroy the cursor without blocking, with query data and the real module lease retained through foreign destructors

#### Scenario: Unknown or invalid custom data
- **WHEN** a relevant entry has unknown bounds, an unsupported/error query, invalid hit data or a null snapshot/query object
- **THEN** the host SHALL preserve completeness and return an explicit incomplete/error state rather than skip the candidate, crash or apply a partial hit; a later valid snapshot SHALL recover
