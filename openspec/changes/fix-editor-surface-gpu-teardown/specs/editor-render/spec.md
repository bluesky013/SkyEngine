## ADDED Requirements

### Requirement: Safe editor surface retirement

The editor renderer SHALL stop recording new work for a closed floating or preview surface, detach its CPU registrations, and wait until submitted GPU and presentation work has completed before releasing its GPU resources. Closing a surface SHALL NOT invalidate pointers or resources held by an already-recording frame.

#### Scenario: Native floating close while the previous frame is in flight
- **WHEN** a floating window closes while its prior submission or presentation may still use its resources
- **THEN** its font and window-event registrations SHALL be detached
- **AND** its UI renderer and swapchain resources SHALL remain alive until the device is idle at a frame boundary
- **AND** its panel-close notification SHALL run after retirement

#### Scenario: Explicit destruction from a paint callback
- **WHEN** a paint callback requests explicit destruction of a floating surface during frame recording
- **THEN** the surface SHALL remain alive through that frame's submission and presentation if already recorded
- **AND** subsequent surface recording SHALL skip it
- **AND** retirement SHALL NOT emit the native-close re-docking notification

#### Scenario: Preview close
- **WHEN** the preview window receives a close request
- **THEN** the renderer SHALL stop acquiring it for subsequent recording
- **AND** its viewport and window resources SHALL be released only after pending device work completes

#### Scenario: Shutdown requested during recording
- **WHEN** a GUI callback requests renderer shutdown during frame recording
- **THEN** CPU subscriptions SHALL be detached and new surface creation/recording SHALL stop
- **AND** resource destruction SHALL wait until the current frame completes submission/presentation and the device becomes idle

### Requirement: Owned per-window event registrations

Each editor-owned window SHALL have an independently owned window-event subscription that preserves the window/event identity and is detached before its renderer target or native window is destroyed. Detaching one window SHALL NOT remove or retain another window's subscription accidentally.

#### Scenario: Close one of several windows
- **WHEN** one window's subscription is detached
- **THEN** later events for that window SHALL NOT invoke the renderer target
- **AND** other live windows SHALL continue forwarding their original close/move/resize identities

#### Scenario: Owner shutdown
- **WHEN** the editor renderer shuts down with multiple windows registered
- **THEN** every subscription SHALL be detached before the renderer target is destroyed
- **AND** no later event broadcast for those window keys SHALL invoke the old target
