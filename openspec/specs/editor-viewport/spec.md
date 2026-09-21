# editor-viewport Specification

## Purpose
TBD - created by archiving change editor-shell-redesign. Update Purpose after archive.
## Requirements
### Requirement: Aurora-backed viewport composited into the main window
The editor viewport SHALL render through Aurora and SHALL be composited into the main window's swapchain
(topology (1)); it SHALL NOT create a child OS window with its own swapchain.

#### Scenario: Viewport shares the editor frame
- **WHEN** a frame is rendered with a viewport open
- **THEN** the viewport output SHALL be composited into the same swapchain as the editor UI, without a separate
  child native window

### Requirement: Viewport presents from the native handle
The viewport SHALL create its presentation target from the main window's native handle via
`SwapChain::Descriptor.window`, owned by a `ClientViewport`.

#### Scenario: ClientViewport initialized from handle
- **WHEN** the viewport is created for the main window
- **THEN** a `ClientViewport` SHALL be initialized from the window's native handle (`HWND` or `CAMetalLayer`) and
  SHALL present on resize

### Requirement: Editor viewport overlays
The viewport SHALL support the editor camera and optional overlays (gizmo, profiler) rendered within the same
frame.

#### Scenario: Gizmo overlay drawn
- **WHEN** the gizmo overlay is enabled and an object is selected
- **THEN** the gizmo SHALL be drawn over the viewport output in the same frame

