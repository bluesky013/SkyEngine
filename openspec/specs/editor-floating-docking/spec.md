# editor-floating-docking Specification

## Purpose
TBD - created by archiving change editor-interactive-docking. Update Purpose after archive.
## Requirements
### Requirement: Floating panel surface
A floating panel SHALL be presented in its own operating-system window with its own UI context, swapchain, and
UI renderer, and SHALL render the same panel view that it renders when docked (including shared-device content
such as the viewport placeholder). Floating windows SHALL use the one editor Aurora device, not a second device,
and any shared content target SHALL be registered in every window that samples it.

#### Scenario: Floated panel draws in its own window
- **WHEN** a panel is floated
- **THEN** its view SHALL be rendered into a dedicated native window's swapchain, not inside the main window

#### Scenario: No extra device
- **WHEN** any number of floating windows exists
- **THEN** they SHALL all use the existing Aurora device

#### Scenario: Shared content is visible in a floating window
- **WHEN** a floating panel samples a content target owned by the editor (for example the viewport placeholder)
- **THEN** that target SHALL have been registered with the floating window's UI renderer and SHALL render

### Requirement: Native window creation order
Floating native windows SHALL respect the backend's graphics-instance creation ordering: on backends that can
only create graphics-capable windows before the device is initialized, floating windows SHALL be pre-created (for
example a hidden pool) at initialization and reused; on-demand creation SHALL be used only where the backend
supports creating a graphics-capable window after device initialization.

#### Scenario: Backend with a creation-order constraint
- **WHEN** a panel is floated on a backend that requires windows to be created before device init
- **THEN** a pre-created hidden window SHALL be shown and bound to the panel instead of creating a new window after
  device init

#### Scenario: On-demand backend
- **WHEN** a panel is floated on a backend that can create a graphics-capable window after device init
- **THEN** a new native window MAY be created on demand

### Requirement: Cross-window drag
Tear-out and re-dock SHALL be driven by pointer capture and screen coordinates, so a drag that leaves the source
window continues to be tracked and can be resolved against the main window or any floating window.

#### Scenario: Drag continues outside the source window
- **WHEN** a tab drag moves the pointer outside the window that started the drag
- **THEN** the drag SHALL continue (pointer capture / screen coordinates) and the drop SHALL be resolved against
  the window under the pointer

#### Scenario: Resolve the drop target
- **WHEN** a dragging panel is released over a floating window
- **THEN** the panel SHALL dock into that window's area, and when released over empty space away from any window
  it SHALL remain or become floating

### Requirement: Tear-out
Dragging a docked panel's tab outside the main window SHALL tear the panel out into a floating window, removing it
from the dock tree and adding it to the floating set.

#### Scenario: Tear out a tab
- **WHEN** the user drags a tab out of the main window and releases outside it
- **THEN** the panel SHALL become floating with its own window and SHALL no longer appear in the main window's dock
  tree

### Requirement: Re-dock
Dragging a floating panel's window (or its header) back over the main window SHALL dock the panel into the
indicated area and release its floating window (destroy it, or return a pre-created window to the pool), restoring
the panel in the dock tree.

#### Scenario: Re-dock a floating panel
- **WHEN** the user drags a floating panel over a dock area of the main window and releases
- **THEN** the panel SHALL be removed from the floating set and inserted into the dock tree at the indicated
  position, and its floating window SHALL be released

### Requirement: Floating window close
Closing a floating window SHALL re-dock its panel into the main window by default rather than losing the panel.
A close request SHALL be broadcast to owners on every supported backend (the SDL-backed surfaces SHALL broadcast
the same close notification as Win32), and closing a floating window SHALL NOT request application exit.

#### Scenario: Close a floating window
- **WHEN** the user closes a floating panel's window
- **THEN** the panel SHALL be re-docked into the main window and SHALL remain visible

#### Scenario: Close broadcast on every backend
- **WHEN** a floating window receives a close request on Windows, macOS, or Linux
- **THEN** the owner SHALL receive the close notification and re-dock the panel

#### Scenario: Closing a floating window does not exit
- **WHEN** a floating window is closed
- **THEN** the application SHALL keep running and only the main window close SHALL request exit

### Requirement: Window geometry notification
The framework SHALL notify owners when a floating window is moved, in addition to resize, so the editor can record
the window's position and size. The layout model SHALL remain the single serialized source of floating geometry,
updated from these notifications.

#### Scenario: Record moved geometry
- **WHEN** a floating window is moved or resized
- **THEN** the editor SHALL update that panel's floating geometry in the layout model and persist it

#### Scenario: Restore uses latest geometry
- **WHEN** the editor restarts after a floating window was moved
- **THEN** the panel SHALL reopen at the last recorded position

### Requirement: Multiple floating windows
The editor SHALL support multiple concurrent floating windows, each with independent content, geometry, and
presentation, and destroying one SHALL not affect the others.

#### Scenario: Several floating panels
- **WHEN** more than one panel is floated
- **THEN** each SHALL have its own window and geometry, and closing one SHALL leave the others intact

#### Scenario: Per-window resize
- **WHEN** a floating window is resized
- **THEN** only its own swapchain SHALL be rebuilt

