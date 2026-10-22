# editor-viewport Specification

## Purpose
TBD - created by archiving change editor-shell-redesign. Update Purpose after archive.
## Requirements
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
