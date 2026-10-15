## ADDED Requirements

### Requirement: Native platform window service parity
Cocoa, Android and UIKit windows SHALL register valid WindowIDs and report the current client pixel size. Cocoa SHALL identify the main window, update its title and geometry, provide cursor services, and publish close notifications before owner destruction. Mobile windows SHALL unregister and detach native callbacks on destruction.

#### Scenario: Main window lookup and title
- **WHEN** the first Cocoa window is registered and its title is changed
- **THEN** NativeWindowManager SHALL find it as the main window and the native title SHALL update

#### Scenario: Close a Cocoa secondary window
- **WHEN** a Cocoa secondary window receives a close request
- **THEN** its keyed close event SHALL fire once without exiting the main application or accessing an owner deleted by a listener

#### Scenario: Native input after owner destruction
- **WHEN** a retained native Cocoa view receives an input or text callback after its C++ window owner is destroyed
- **THEN** the callback SHALL be inert and SHALL NOT dereference the former owner

#### Scenario: Native mobile window lifetime
- **WHEN** an Android or UIKit engine window is created and destroyed
- **THEN** manager lookup SHALL succeed only during its lifetime and native callbacks SHALL NOT retain a dangling owner

### Requirement: Cocoa wheel and native shell services
Cocoa SHALL publish mouse wheel cursor position and fractional delta using the shared event contract, convert text-input rectangles from pixels to native points, and implement the existing desktop file reveal service.

#### Scenario: Trackpad scrolling
- **WHEN** a precise scrolling event has a fractional delta over a Cocoa view
- **THEN** the event SHALL carry the cursor's client pixel coordinates and retain the fractional scrolling delta

#### Scenario: Pixel text input anchor
- **WHEN** the host supplies a framebuffer pixel text-input rectangle on a Retina window
- **THEN** the native IME candidate rectangle SHALL use the corresponding point-space position
