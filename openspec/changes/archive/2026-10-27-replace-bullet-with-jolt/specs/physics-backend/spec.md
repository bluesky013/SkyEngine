## MODIFIED Requirements

### Requirement: Backend lifecycle hooks

The physics backend factory SHALL expose explicit initialization and shutdown hooks invoked during registration and unregistration. Initialization SHALL report failure without exceptions. Shutdown SHALL remove factory access immediately and release backend-owned state only after all world leases that require it have been released.

#### Scenario: Initialize a backend on registration
- **WHEN** a physics backend is registered
- **THEN** its initialization hook SHALL succeed before any physics world or shape is created

#### Scenario: Initialization fails
- **WHEN** backend initialization fails
- **THEN** registration SHALL report failure, no partially initialized backend SHALL be published, and acquired resources SHALL be released

#### Scenario: Shut down a backend on unregistration
- **WHEN** a physics backend is unregistered
- **THEN** new creation SHALL become unavailable and SDK teardown SHALL occur only after its outstanding world leases are released

#### Scenario: A backend with no global state
- **WHEN** a test backend does not require global state
- **THEN** its lifecycle hooks SHALL be satisfiable as no-ops without changing engine behavior

### Requirement: Single active backend registration

The engine registry SHALL maintain exactly one active backend at a time. Registering a new backend SHALL unregister the previous factory and release the registry's ownership of it without invalidating existing worlds' state or code leases. World destruction SHALL use its creating owner's lifetime contract, not whichever backend is currently active.

#### Scenario: Replace the active backend
- **WHEN** a new backend is registered while one is active
- **THEN** the previous backend SHALL stop accepting creation, while outstanding worlds SHALL remain safely destructible

## REMOVED Requirements

### Requirement: Bullet backend implements the extended shape set

**Reason**: Bullet is removed as a shipped provider.

**Migration**: Use the Jolt shape implementation through the existing engine shape descriptors and world interface.

## ADDED Requirements

### Requirement: Jolt implements the engine shape contract

The Jolt backend SHALL implement box, sphere, capsule, heightfield, triangle mesh, convex hull and compound descriptors without exposing SDK objects. It SHALL honor pivots, body-origin transforms and supported scale, validate native creation results, and fail the entire request if a compound child cannot be created.

#### Scenario: Capsule dimensions
- **WHEN** a capsule descriptor supplies radius and cylindrical-section height
- **THEN** the resulting shape SHALL have that radius and cylinder height, excluding end caps

#### Scenario: Heightfield coordinates
- **WHEN** a heightfield is created
- **THEN** horizontal coordinates SHALL be centered using grid extents and vertical coordinates SHALL equal `sample * heightScale + heightOffset` along its declared up axis

#### Scenario: Grid does not fit the native heightfield representation
- **WHEN** a valid rectangular or small grid cannot be represented by the selected native heightfield shape
- **THEN** the backend SHALL cook equivalent static triangle geometry without discarding or inventing terrain samples

#### Scenario: Compound child fails
- **WHEN** a nested child is malformed or unsupported
- **THEN** creation SHALL fail and release all intermediate shapes without publishing a partial compound

#### Scenario: Unsupported dynamic mesh
- **WHEN** a world requests dynamic triangle-mesh support in this delivered configuration
- **THEN** creation or option validation SHALL report Unsupported rather than silently creating an invalid dynamic mesh

### Requirement: Native capacity is explicit and distinct from gameplay budgets

World creation SHALL accept validated native resource limits with finite defaults. A zero gameplay max-body or max-active-body budget SHALL mean no additional gameplay restriction, not unlimited native allocation. Unsupported growth, out-of-range limits and native exhaustion SHALL return explicit failure.

#### Scenario: Body capacity is reached
- **WHEN** creation exceeds the world's configured native body capacity
- **THEN** creation SHALL return an invalid handle with a capacity error and existing objects SHALL remain valid

#### Scenario: Failed option update
- **WHEN** an option update exceeds fixed native capacity or requests unsupported behavior
- **THEN** it SHALL return a failure status and leave the previously accepted options unchanged

### Requirement: Full collision masks survive native filtering

The backend SHALL preserve all 32 bits of collision group/mask data and apply the existing bilateral FilterAccepts rule to simulation. Query filtering SHALL apply the full mask and all ignored handles. Runtime filter changes SHALL invalidate affected cached contact decisions.

#### Scenario: Upper collision bits are used
- **WHEN** two objects interact through bits above bit 15
- **THEN** collision and query acceptance SHALL follow the engine masks without truncation

#### Scenario: An existing contact becomes filtered
- **WHEN** a live body's filter changes to reject a current contact
- **THEN** subsequent simulation SHALL stop that interaction and emit the appropriate end event without requiring body recreation

### Requirement: Live body property edits preserve identity transactionally

Supported mass and shape edits SHALL validate and cook candidates before changing the live body. Success SHALL retain the body handle, velocity and connected constraint handles, updating inertia and constraint COM offsets as required. Failure SHALL preserve the old component data and body and SHALL reach the editor command result.

#### Scenario: Invalid shape edit
- **WHEN** an edit supplies invalid geometry or an unavailable mesh product
- **THEN** the edit SHALL fail without destroying the previous body or entering undo history as a successful edit

#### Scenario: Constrained moving body is edited
- **WHEN** a valid mass, shape or scale change affects a constrained body
- **THEN** existing handles and constraints SHALL remain valid and their COM-dependent anchors SHALL be adjusted
