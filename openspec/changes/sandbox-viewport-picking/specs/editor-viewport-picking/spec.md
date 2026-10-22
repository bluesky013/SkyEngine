## ADDED Requirements

### Requirement: Shared single Actor selection by viewport click
The editable viewport SHALL select the nearest supported static mesh Actor on an eligible left click and SHALL clear selection on a complete miss, using SelectionService as its sole selection owner. Selection SHALL update the Outliner, Inspector and Gizmo without changing transforms, World dirty state or command history. Q and transform tools SHALL share this behavior outside their handles.

#### Scenario: Select a mesh without a collider
- **WHEN** an eligible click hits a supported static mesh Actor that has no physics collider
- **THEN** SelectionService SHALL select that Actor and all selection consumers SHALL reflect it

#### Scenario: Click empty scene content
- **WHEN** a complete supported query returns no hit for an eligible click inside the scene content
- **THEN** the current selection SHALL be cleared without a World command or dirty state change

### Requirement: Precise geometric nearest hit
Picking SHALL use bounds only as a broad phase and SHALL confirm static triangle intersections before returning a hit. It SHALL honor the supported visibility, clipping and culling semantics, compare distances in world space and support parent transforms, invertible non-uniform scales and negative scales. Invalid geometry or singular transforms SHALL have explicit outcomes. Unsupported material or deformation semantics SHALL NOT be presented as pixel-accurate results.

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
