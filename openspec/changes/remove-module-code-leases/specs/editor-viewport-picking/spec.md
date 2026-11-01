## MODIFIED Requirements

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
- **THEN** new collection SHALL stop, its generation SHALL invalidate dependent results, and required provider/data owners SHALL remain alive until work and destructors are safely retired; defining DLL code SHALL remain process resident

#### Scenario: Transform Gizmo consumes a click
- **WHEN** an existing IGizmoVisualProvider handle captures manipulation
- **THEN** the scene provider system SHALL NOT issue an Actor-selection click or replace the existing handle drawing/dragging contract

### Requirement: Provider-owned geometry queries
EditorCore SHALL expose a custom geometry query interface through the existing scoped provider snapshot. Entries SHALL bind an owning Actor, stable generation, optional conservative world bounds and immutable query ownership. Providers MAY retain their own BVH or spatial index. The host SHALL apply visibility, selection and occlusion filters, perform bounded bounds screening, and compose exact world-space hits with the existing mesh/proxy resolver. VisibleSurface SHALL remain unsupported by this CPU extension.

#### Scenario: Custom geometry participates without a mesh payload
- **WHEN** a provider returns a supported exact surface hit from its own prepared query object
- **THEN** the picker SHALL validate distance, optional surface data and entry identity, resolve GeometrySurface semantics, and select only the owning Actor without copying or rebuilding the provider's spatial index

#### Scenario: A custom query yields and is cancelled
- **WHEN** a query exhausts its cooperative work/time budget or its request is cancelled, replaced, invalidated or unregistered
- **THEN** Pending SHALL retain a provider-owned continuation only while valid; cancellation SHALL destroy the cursor without blocking, with query data and provider/geometry owners retained through foreign destructors while defining DLL code stays process resident

#### Scenario: Unknown or invalid custom data
- **WHEN** a relevant entry has unknown bounds, an unsupported/error query, invalid hit data or a null snapshot/query object
- **THEN** the host SHALL preserve completeness and return an explicit incomplete/error state rather than skip the candidate, crash or apply a partial hit; a later valid snapshot SHALL recover
