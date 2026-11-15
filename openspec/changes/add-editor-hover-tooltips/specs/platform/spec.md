## ADDED Requirements

### Requirement: Native desktop hover-leave notification

The desktop native window backend SHALL expose ordinary pointer departure from the client view through a default-empty `IMouseEvent::OnMouseLeave(WindowID)` callback. Cocoa tracking-area exit, Win32 mouse-leave tracking and X11 normal leave crossings SHALL identify the originating live window. The notification SHALL NOT fabricate pointer coordinates, release pressed buttons, cancel pointer capture or replace existing drag routing. Existing mouse-event consumers SHALL remain source-compatible without implementing the optional callback.

#### Scenario: Pointer leaves a focused window
- **WHEN** an uncaptured pointer leaves a desktop window's client view while the window remains focused
- **THEN** the native backend broadcasts mouse leave with that window's WindowID
- **AND** the editor can dismiss that surface's tooltip without waiting for focus loss or another pointer motion

#### Scenario: Leave preserves gesture routing
- **WHEN** a native leave notification is delivered while a gesture is active
- **THEN** the notification itself does not alter capture or button state
- **AND** subsequent native drag and button-up input retains its existing dispatch path

#### Scenario: Tracking registration fails
- **WHEN** Win32 mouse-leave tracking registration fails
- **THEN** the failure is reported and a later eligible mouse motion can retry
- **AND** ordinary mouse motion is still delivered

#### Scenario: Native responder outlives its owner
- **WHEN** an AppKit-retained view receives mouse exit after its C++ window owner is destroyed
- **THEN** it ignores the delayed callback without accessing the retired owner or broadcasting its former window identity

#### Scenario: X11 capture transition is not a physical leave
- **WHEN** X11 reports a crossing caused by pointer grab/ungrab or movement into a child window
- **THEN** it does not broadcast an ordinary window mouse-leave notification
