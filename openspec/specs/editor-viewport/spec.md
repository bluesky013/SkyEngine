# editor-viewport Specification

## Purpose

Define viewport ownership, world rendering, play sessions, picking, highlighting and transform tools.

## Scope notes

### editor-selection-highlight

Define editor selection silhouettes, subdued occluded feedback, fallback visuals, and bounded generic rendering inputs without asset or material mutation.

### editor-transform-gizmo

Define customizable viewport transform handles, shared geometry and hit testing, and cancelable editing integrated with selection and undo history.

### editor-viewport-picking

Define editor viewport Actor selection, bounded geometry and provider queries, gesture arbitration, and consistent visual/pick ordering without runtime editor dependencies.

## Requirements

### Requirement: Duplicate world for play

The editor SHALL run the game on a world **duplicated** from the edit world, so play never mutates the authored
world. The duplicate SHALL include the actors/components and the subsystem set (from the world description).

#### Scenario: Play duplicates
- **WHEN** the user starts Play
- **THEN** a runtime world SHALL be created as a copy of the edit world (actors/components + subsystems) and
  run, while the edit world is left intact

#### Scenario: Edit world is not ticked
- **WHEN** the game is playing
- **THEN** only the runtime world SHALL advance; the edit world SHALL NOT be ticked or mutated

#### Scenario: Subsystems run in the runtime world
- **WHEN** Play starts and the world enables a subsystem (e.g. physics)
- **THEN** the runtime world's subsystem SHALL be started, independently of the edit world's subsystems

### Requirement: Play / Pause / Stop

The editor SHALL provide Play, Pause, and Stop controls. Play runs the runtime world; Pause suspends it; Stop ends
the session and discards the runtime world.

#### Scenario: Pause and resume
- **WHEN** the user pauses and then plays again
- **THEN** the runtime world SHALL continue from where it paused (same world), not restart

#### Scenario: Stop discards
- **WHEN** the user stops
- **THEN** the runtime world SHALL be discarded and the editor SHALL return to editing the (unchanged) edit world

#### Scenario: Play without a world
- **WHEN** the user requests Play while no world document is open
- **THEN** no session SHALL start and the editor SHALL remain in the editing state

### Requirement: Play state is visible

The editor SHALL reflect the current play state (Editing / Playing / Paused) so the user can tell whether the
world is running.

#### Scenario: State surface
- **WHEN** the play state changes
- **THEN** the editor SHALL update its play-state indicator accordingly

### Requirement: Session does not outlive its document

Opening, creating, or closing a world SHALL stop any running play session first.

#### Scenario: Opening or closing a world during play
- **WHEN** the user opens, creates, or closes a world while a session is running
- **THEN** the running session SHALL be stopped before the document changes

### Requirement: Editor renderer initialization from the main window

`EditorRender` SHALL initialize from the editor main window's native handle: it SHALL obtain the Aurora device
(reusing an existing one rather than creating a second) and create a `ClientViewport`/swapchain for that window.
It SHALL accept the RHI `API` to initialize with (defaulting to `API::DEFAULT`), so the editor host can select the
backend.

#### Scenario: Initialize from the window handle
- **WHEN** `EditorRenderer::Init` is called with the editor main window handle and size
- **THEN** an Aurora device SHALL be available and a swapchain-backed viewport SHALL be created for that window

#### Scenario: No second device
- **WHEN** an Aurora device already exists
- **THEN** `EditorRender` SHALL reuse it and SHALL NOT create a second device

#### Scenario: Backend selected by the host
- **WHEN** the editor host passes `--rhi dx12`
- **THEN** `EditorRenderer` SHALL initialize Aurora with `API::DX12`

### Requirement: Clear and present frame loop

`EditorRender` SHALL run a per-frame loop that acquires a backbuffer, clears it, submits, and presents it to the
editor window, tolerating a null backbuffer or zero extent by skipping the frame. It SHALL check the surface
extent BEFORE acquiring so a zero-size surface does not acquire an image it cannot present, and SHALL release an
acquired image that turns out unusable instead of leaving it unreleased. It SHALL react to window lifecycle
events so it never acquires a window that is closed: a closed preview window's viewport SHALL be dropped.

#### Scenario: Present a cleared frame
- **WHEN** a frame is rendered with a valid backbuffer
- **THEN** the backbuffer SHALL be cleared to the configured color and presented to the editor window

#### Scenario: Skip invalid frames
- **WHEN** the backbuffer is null or the extent is zero (for example while minimized or out of date)
- **THEN** the loop SHALL skip rendering that frame without crashing

#### Scenario: Zero extent does not acquire
- **WHEN** the surface extent is zero at the start of a frame
- **THEN** the loop SHALL skip acquiring a backbuffer for that frame

#### Scenario: Unusable acquired image is not leaked
- **WHEN** an image was acquired but no backbuffer is available
- **THEN** the loop SHALL release the acquired image before returning

#### Scenario: Closed preview window is not acquired
- **WHEN** the standalone preview window receives a close request
- **THEN** the renderer SHALL drop that window's viewport and SHALL NOT acquire it again

### Requirement: GUI pipeline over sky::ui draw data

`EditorRender` SHALL provide a GUI pipeline that consumes `sky::ui` `UIDrawData` and draws it with batching and
clipping (alpha-blended, depth off), covering solid quads and images at minimum.

#### Scenario: Draw a quad from draw data
- **WHEN** a minimal `sky::ui` document produces draw data containing a quad
- **THEN** the GUI pipeline SHALL draw that quad visibly over the cleared frame

#### Scenario: Batch and clip
- **WHEN** draw data contains multiple elements with a clip rect
- **THEN** the pipeline SHALL batch compatible draws and SHALL restrict drawing to the clip region

### Requirement: Independence from the engine scene pipeline

`EditorRender` SHALL be independent of the engine scene pipeline: it SHALL NOT require `RenderScene` or the engine
top-level renderer to draw the editor UI.

#### Scenario: UI without the scene pipeline
- **WHEN** the editor starts and draws its UI
- **THEN** the UI SHALL render without invoking the engine scene renderer

### Requirement: Render host and module dependencies

The RHI-linked render host SHALL be a target separate from the RHI-free sandbox shell, and the RHI modules it
uses SHALL be registered in the module deploy dependencies.

#### Scenario: Shell stays RHI-free
- **WHEN** the sandbox shell target is built
- **THEN** it SHALL NOT link the RHI, and the render host SHALL link `EditorRender`

### Requirement: Shell-driven GUI content

`EditorRender` SHALL draw the editor shell's `sky::ui` draw data each frame (`UIContext` layout + paint into the
GUI pipeline) and SHALL NOT contain hardcoded mock UI (placeholder rectangles or labels).

#### Scenario: Drawn from the shell context
- **WHEN** a frame is rendered
- **THEN** the GUI content SHALL come from the shell's `UIContext` (`Layout()` + `Paint()`) and its draw data

#### Scenario: Layout changes are reflected
- **WHEN** the layout model or panel set changes
- **THEN** the rendered content SHALL change accordingly, with no code change in `EditorRender`

### Requirement: Editor selection feedback

The editor SHALL display selection feedback for live selected Actors regardless of whether selection originated in the viewport or outliner. Feedback SHALL use the editor theme and SHALL NOT mutate world data, materials or undo history.

#### Scenario: Select and clear
- **WHEN** the selection changes to a supported static mesh and is subsequently cleared
- **THEN** the corresponding mesh receives a screen-space outline and that outline disappears after clearing

#### Scenario: Document and play transitions
- **WHEN** the document changes, the Actor is deleted, or the editor enters play mode
- **THEN** obsolete editor selection feedback is removed and reused renderer identities do not inherit it

### Requirement: Depth-aware silhouette

Selected supported geometry SHALL produce a union silhouette with stronger visible and subdued occluded feedback. The renderer SHALL reuse the existing scene depth, redraw only selected geometry for the mask, and skip all highlight passes for an empty selection.

#### Scenario: Occlusion
- **WHEN** an unselected opaque mesh partially obscures a selected mesh
- **THEN** visible edges use the normal selection color and occluded edges use the configured lower opacity

#### Scenario: No selection cost
- **WHEN** no eligible render entities are selected
- **THEN** no highlight mask, highlight depth target or outline composition pass is generated

### Requirement: Fallback and ownership

Unsupported and non-mesh selections SHALL use available projected bounds or a pivot marker; selected overlay proxies SHALL use owner-associated feedback. Generic rendering interfaces SHALL NOT depend on editor selection types. Geometry query support alone SHALL NOT be interpreted as a renderable silhouette contract.

#### Scenario: Custom query without render geometry
- **WHEN** a selected custom geometry entry supplies bounds but no renderable mesh
- **THEN** the editor displays bounds feedback without trying to decode or render its private BVH

### Requirement: Bounded validated rendering inputs

Highlight input identities, counts and finite style values SHALL be validated. Accepted frame data and shader objects SHALL remain owned until normal renderer retirement.

#### Scenario: Malformed input
- **WHEN** highlight inputs contain invalid identities, non-finite colors or an unsupported width
- **THEN** an explicit validation error is returned before recording highlight draws

### Requirement: Shared selection and transform handles

The editor SHALL expose translate, rotate and scale tools for exactly one selected Actor with a valid Transform in the active edit World, using SelectionService as the sole selection owner. Translation SHALL support axis and plane constraints, rotation SHALL support axis constraints, and scaling SHALL support local axis and uniform constraints. Translation and rotation SHALL support World and Local orientation. Scaling SHALL remain Local and disable the space toggle.

#### Scenario: Selected Actor is edited
- **WHEN** an Actor is selected in the Outliner and the user drags a translation axis
- **THEN** the viewport and Inspector SHALL reflect the same Actor's constrained Transform

#### Scenario: Ineligible selection
- **WHEN** selection is empty, multiple, an asset, detached, or lacks a Transform
- **THEN** transform editing SHALL be disabled without modifying any Actor

#### Scenario: Parent and local orientation
- **WHEN** a child of a rotated and invertibly scaled parent is manipulated
- **THEN** its local Transform SHALL be derived under the existing TRS convention while preserving unrelated components and parentage

### Requirement: Customizable gizmo drawing contract

Gizmo geometry generation SHALL be replaceable through an injected interface whose public data contains no UI toolkit or GPU types. The default provider SHALL implement all supported handles. Drawing and hit testing SHALL use the same owned geometry and stable handle identifiers. Replacing the provider SHALL cancel active manipulation before releasing the old provider.

#### Scenario: Custom appearance
- **WHEN** the host injects a provider that changes a translation handle's geometry
- **THEN** the new shape SHALL be rendered and hit tested at its new location while retaining the same translation semantics

#### Scenario: Hidden or invalid handle
- **WHEN** a provider omits a handle or emits non-finite geometry
- **THEN** omitted geometry SHALL not be hittable and invalid output SHALL return an explicit failure without applying a transform

#### Scenario: Filled contour closure
- **WHEN** a custom provider emits a filled convex polygon without the closed-polyline flag
- **THEN** drawing and hit testing SHALL treat the fill as closed, including tolerance around its final edge

### Requirement: Relative snapping and valid transforms

Snapping SHALL quantize translation, rotation and scale deltas relative to the drag start using configurable positive steps. Scaling SHALL preserve initial component signs and SHALL NOT cross zero. Invalid/non-finite transforms and non-invertible parent transforms SHALL be rejected explicitly. Degenerate drag projections SHALL retain the last valid value.

#### Scenario: Stable snap from an off-grid origin
- **WHEN** a translated Actor starts off-grid and is dragged with snapping enabled
- **THEN** movement SHALL be an integer multiple of the configured step from its starting position, without an initial jump

#### Scenario: Singular parent
- **WHEN** the target's parent has a zero scale component
- **THEN** manipulation SHALL be refused with an explicit reason and both world and local transforms SHALL remain unchanged

### Requirement: Cancelable preview and single-step history

A gesture SHALL preview without adding history or dirtying the World, then commit exactly one before/after local Transform command for a nonzero change. Cancel and no-op gestures SHALL add no command. Undo and redo SHALL restore exact values and mark the document dirty. The implementation SHALL resolve Actor identity without retaining a dangling component pointer.

#### Scenario: Drag then undo and redo
- **WHEN** a gesture produces multiple intermediate previews and is released
- **THEN** exactly one undo step SHALL be created, Undo SHALL restore the initial local Transform, and Redo SHALL restore the final local Transform

#### Scenario: Minimum rotation snap
- **WHEN** a rotation gesture produces the supported minimum snap of 0.1 degrees
- **THEN** release SHALL retain that change as one undoable command rather than classifying it as a no-op

#### Scenario: Cancel preview
- **WHEN** Escape, focus/capture loss, a modal, a document switch, target invalidation, parent change, save, history traversal, or Play interrupts a gesture
- **THEN** the valid target SHALL be restored before the next operation, capture SHALL be released, and no preview command or new dirty state SHALL remain

#### Scenario: Target already released
- **WHEN** cancellation discovers that the target identity is no longer valid
- **THEN** it SHALL release the interaction without accessing old component storage

### Requirement: Unified tool actions

Tool controls SHALL use EditorActionRegistry and shared ActionBarBuilder/ToolBar/MenuBar widgets with SVG icons, themed metrics and shared enabled/checked state. Q/W/E/R SHALL select the pointer/translate/rotate/scale tools, X SHALL switch orientation, G SHALL toggle snapping, and Escape SHALL cancel the active gesture. Numeric settings SHALL use reflected data and ReflectedFormView.

#### Scenario: Consistent entry points
- **WHEN** the user changes tools through a toolbar action or its eligible shortcut
- **THEN** the same controller state and checked indication SHALL update

#### Scenario: Scene persistence
- **WHEN** a committed manipulation is saved and the World is reopened
- **THEN** the Actor SHALL retain the committed local Transform and parent reference

### Requirement: Aurora-backed viewport composited into the main window

The editor viewport SHALL render through Aurora and SHALL present according to its own `presentation`: `TEXTURE`
(render to an offscreen target composited by the editor UI, without creating a separate OS window) or `WINDOW`
(its own native window + swapchain). The viewport SHALL own its presentation and SHALL be able to change it while
preserving its content target. The default main viewport SHALL use `TEXTURE`.

#### Scenario: Default texture presentation
- **WHEN** a frame is rendered with the main viewport open
- **THEN** the viewport output SHALL be composited as a UI image in the main window's frame, without a separate
  child native window

#### Scenario: Window presentation
- **WHEN** a viewport uses `WINDOW` presentation
- **THEN** it SHALL create a native window and a `ClientViewport`/swapchain and present there

#### Scenario: Switch presentation
- **WHEN** a viewport switches between `TEXTURE` and `WINDOW`
- **THEN** the window/swapchain SHALL be created or destroyed while the content target is preserved

### Requirement: Viewport presents from the native handle

The viewport SHALL create its presentation target from the main window's native handle via
`SwapChain::Descriptor.window`, owned by a `ClientViewport`.

#### Scenario: ClientViewport initialized from handle
- **WHEN** the viewport is created for the main window
- **THEN** a `ClientViewport` SHALL be initialized from the window's native handle (`HWND` or `CAMetalLayer`) and
  SHALL present on resize

### Requirement: Editor viewport overlays

The viewport SHALL support the editor camera and optional overlays (gizmo, profiler) rendered within the same frame. Gizmo projection, drawing and hit testing SHALL use the scene camera snapshot and actual image content rectangle with consistent surface/DPI mapping. The gizmo SHALL be a clipped topmost overlay with theme-defined size and colors, and SHALL be suppressed for pivots behind or inside the near plane. This change SHALL NOT require a profiler implementation.

#### Scenario: Gizmo overlay drawn
- **WHEN** the gizmo overlay is enabled and an object is selected
- **THEN** the gizmo SHALL be drawn over the viewport output in the same frame

#### Scenario: Docking and DPI changes
- **WHEN** the viewport is resized or moved to a floating surface with another DPI scale
- **THEN** drawing and hit regions SHALL align with the displayed scene and SHALL remain clipped to the content rectangle

#### Scenario: Pending frame remains immutable
- **WHEN** the renderer retains a busy pending frame while further input arrives
- **THEN** its scene camera and overlay payload SHALL remain unchanged and new state SHALL be captured for a later frame

#### Scenario: Offscreen projected handles
- **WHEN** camera rotation projects default or custom Gizmo geometry far outside the image
- **THEN** visible triangles SHALL be geometrically clipped before snapshot serialization, entirely offscreen triangles SHALL be omitted, and finite offscreen geometry SHALL NOT terminate the editor

#### Scenario: Rendering falls behind input
- **WHEN** two submitted editor frames await completion
- **THEN** the host SHALL continue input and event processing while deferring snapshot production until an exact frame completion acknowledgement, preserving ordered commands and using current camera state for the next snapshot

### Requirement: Independent editor camera navigation

Edit mode SHALL use an editor-owned perspective camera independently of scene CameraComponents. RMB dragging SHALL control yaw/pitch; held RMB plus WASD/QE SHALL move forward/back/left/right/down/up with frame-time integration and normalized diagonal speed; Shift SHALL temporarily accelerate. MMB SHALL pan, wheel SHALL dolly and Alt+LMB SHALL orbit. F SHALL aim at the uniquely selected Actor's pivot at a valid orbit distance. Navigation SHALL NOT mutate scene objects, dirty the World, or add World history.

#### Scenario: Frame-rate independent flight
- **WHEN** equal input is integrated over the same elapsed duration using different frame intervals
- **THEN** travel distance SHALL agree within numerical tolerance and diagonal travel SHALL not be faster

#### Scenario: Pitched forward and vertical flight
- **WHEN** forward/backward and world-vertical keys are combined while the camera is pitched
- **THEN** resulting world-space travel speed SHALL equal the configured speed and active boost, independent of pitch and key combination

#### Scenario: Scene camera exists
- **WHEN** a scene CameraComponent exists and the user navigates in Edit mode
- **THEN** the editor camera SHALL drive the view without changing that component

#### Scenario: Stop restores edit view
- **WHEN** Play or Paused ends
- **THEN** the pre-existing editor camera SHALL resume control and no navigation keys SHALL remain held

### Requirement: Toolbar camera speed control

The toolbar SHALL expose speed adjustment actions and current speed feedback backed by the same bounded positive speed setting as the reflected controls panel. Speed changes SHALL affect subsequent navigation immediately, SHALL remain session settings and SHALL NOT enter World history. Invalid speed values SHALL be rejected or normalized to the declared bounds with visible current state.

#### Scenario: Change speed during authoring
- **WHEN** the user increases camera speed through the toolbar and starts flying
- **THEN** movement SHALL use the updated speed and the control feedback SHALL show that value

#### Scenario: Temporary boost
- **WHEN** Shift is released during flight
- **THEN** speed SHALL return to the configured base value without changing the stored setting

### Requirement: Viewport input ownership and shortcut eligibility

Viewport shortcuts SHALL be eligible only for the focused editable viewport when no text field, modal, menu or competing gesture owns input. Eligibility SHALL be independent of action enabled state so an ineligible shortcut does not consume text. Camera flight SHALL own WASD/QE before tool accelerators. Main and floating windows SHALL apply the same rules using the event's originating surface.

#### Scenario: Typing tool letters
- **WHEN** W, E, R, Q, X, G or F is typed in an Inspector or dialog field
- **THEN** the field SHALL receive input and neither tools nor the camera SHALL change

#### Scenario: Fly forward without changing tools
- **WHEN** RMB navigation is active and W is held
- **THEN** the camera SHALL move forward and the active gizmo tool SHALL remain unchanged

#### Scenario: Captured drag leaves content
- **WHEN** a gizmo or navigation gesture moves outside the image before release
- **THEN** its originating pointer SHALL remain routed to the owner until release or cancellation

#### Scenario: Another mouse button is pressed during a gesture
- **WHEN** another button is pressed and released while the initiating button remains held
- **THEN** focus and UI/native capture SHALL stay with the gesture owner, the extra button edges SHALL NOT start another interaction, and the initiating button's release outside the viewport SHALL end the gesture

#### Scenario: Focus or context is lost
- **WHEN** focus is lost, a modal/menu opens, the viewport is detached, its geometry changes during a gesture, the document changes, or Play begins
- **THEN** pointer and keyboard interaction state SHALL be cleared and further scene editing SHALL require a fresh eligible gesture

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
- **THEN** new collection SHALL stop, its generation SHALL invalidate dependent results, and required provider/data owners SHALL remain alive until work and destructors are safely retired; defining DLL code SHALL remain process resident

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
- **THEN** Pending SHALL retain a provider-owned continuation only while valid; cancellation SHALL destroy the cursor without blocking, with query data and provider/geometry owners retained through foreign destructors while defining DLL code stays process resident

#### Scenario: Unknown or invalid custom data
- **WHEN** a relevant entry has unknown bounds, an unsupported/error query, invalid hit data or a null snapshot/query object
- **THEN** the host SHALL preserve completeness and return an explicit incomplete/error state rather than skip the candidate, crash or apply a partial hit; a later valid snapshot SHALL recover

### Requirement: Editable World reaches the renderer through one producer

The editor SHALL extract the active World through the owned scene bridge after simulation tick and before the host seals its packet. The editor SHALL retain the same packet when the renderer reports Busy.

#### Scenario: Transform edit updates the viewport
- **WHEN** a user edits a scene component's transform through the reflected inspector
- **THEN** the next accepted host packet contains the updated world transform and the viewport renders that transform

#### Scenario: Scene orientation across output backends
- **WHEN** a scene object is translated along the camera's positive Y axis
- **THEN** it appears above the image center in Vulkan and DX12 with HDR both enabled and disabled, using the scene Pass's canonical Y-up projection and the existing logical output transform

### Requirement: Scene editing preserves logical data and ownership

Scene create/delete actions SHALL use the shared action registry and undo service. Persisted data SHALL contain logical asset UUIDs and component values, and SHALL NOT contain renderer or native GPU pointers.

#### Scenario: Save and reopen a primitive
- **WHEN** a user creates a Cube, edits its transform, saves the World and reopens it
- **THEN** the same logical mesh UUID and edited transform are restored and rendered through the production resource source

#### Scenario: Delete and undo
- **WHEN** a user deletes a scene object and then undoes the action
- **THEN** the editor retains the actor's authored data for undo and the bridge observes a new renderer lifetime without reusing an unretired generation

### Requirement: Inspector bindings remain valid across document changes

The editor SHALL invalidate reflected bindings and history before releasing the previous document's component storage. Both const and mutable getters SHALL capture previous values for undo.

#### Scenario: Switch documents
- **WHEN** the active document is replaced or closed
- **THEN** selection and undo history no longer reference the previous document's components

### Requirement: Scene hierarchy and component inspection remain separate

The Outliner SHALL display Actors with their Transform parent/child hierarchy and SHALL NOT display components as selectable scene nodes. Selecting an Actor SHALL display its inspectable components as grouped sections in the existing reflected Inspector, with Transform first. Field edits, defaults and expansion state SHALL remain independent across component sections, including identically named fields. Scene deletion SHALL target an Actor selection.

#### Scenario: Inspect an Actor with several components
- **WHEN** an Actor with Transform and StaticMesh components is selected in the Outliner
- **THEN** the Inspector displays both component sections, and editing or undoing a StaticMesh property affects only that Actor's StaticMesh

#### Scenario: Child appears before its parent in storage
- **WHEN** a child Actor precedes its parent in World storage
- **THEN** the Outliner displays the parent followed by the indented child using their real Actor IDs

#### Scenario: Selected Actor is removed
- **WHEN** the selected Actor is detached or its World is unbound
- **THEN** all Inspector component bindings are cleared before the previous storage is released

### Requirement: Targeted editor module builds refresh runtime dependencies

The build SHALL use the existing editor module configuration and dependency helpers to order enabled runtime module targets. Building an editor module SHALL rebuild its declared runtime dependencies without linking those DLLs into the consumer or requiring the host executable target to be selected.

#### Scenario: Shader reflection layout changes
- **WHEN** shader reflection types change and SandboxModule is built directly
- **THEN** its configured Aurora.ShaderCompiler dependency is rebuilt before SandboxModule so the running editor consumes matching reflection layouts
