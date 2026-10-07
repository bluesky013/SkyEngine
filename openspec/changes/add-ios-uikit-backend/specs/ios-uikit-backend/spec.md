# Delta: ios-uikit-backend

## ADDED Requirements

### Requirement: UIKit window and event backend

The framework SHALL provide an iOS window/event backend based on UIKit behind the existing
`NativeWindow`/`PlatformBase` interfaces. The content view SHALL be a `UIView` whose backing layer class is
`CAMetalLayer`, and `GetNativeHandle()` SHALL return that layer. Native handles SHALL be exposed as `void*`
(UIKit types confined to the implementation), and the implementation SHALL be built with ARC.

#### Scenario: Surface handle

- **WHEN** an `IOSWindow` is created
- **THEN** its view's backing layer SHALL be a `CAMetalLayer` and `GetNativeHandle()` SHALL return it

### Requirement: UIKit-driven frame loop

The frame loop SHALL be driven by `CADisplayLink` (UIKit owns the run loop); the platform SHALL NOT block in an
`NSApplication`-style loop. `PollEvent` SHALL coexist with UIKit's event callbacks rather than pumping the loop.

#### Scenario: Display-link frames

- **WHEN** the app is on screen
- **THEN** engine frames SHALL be ticked from a `CADisplayLink` callback

#### Scenario: Background suspend

- **WHEN** the app resigns active / enters background
- **THEN** the display link SHALL stop (no leaked `CAMetalDrawable`), and resume on foreground

### Requirement: UIKit input and DPI

The backend SHALL map `touchesBegan/Moved/Ended/Cancelled` to engine input events and soft-keyboard input
(`UIKeyInput`/`UIResponder`) to `IKeyboardEvent` (`OnKeyDown/Up`, `OnTextInput`). `GetDpiScale()` SHALL report the
`UIScreen` scale.

#### Scenario: Touch to engine events

- **WHEN** the user touches the screen
- **THEN** the backend SHALL emit the corresponding engine input event(s)

#### Scenario: Soft keyboard text

- **WHEN** the user types on the soft keyboard
- **THEN** `OnTextInput` SHALL be emitted (and key events for non-text keys)

#### Scenario: DPI scale

- **WHEN** the display scale is queried
- **THEN** `GetDpiScale()` SHALL report the current `UIScreen` scale

### Requirement: iOS CMake branch

The framework build SHALL select its platform sources per OS: `Darwin`/macOS SHALL use `platform/macos/*`, and
iOS SHALL use `platform/ios/*`, linking `UIKit`/`QuartzCore`/`Metal`/`Foundation` and enabling ARC for the iOS
sources.

#### Scenario: iOS configure

- **WHEN** configured with `CMAKE_SYSTEM_NAME=iOS`
- **THEN** the framework SHALL compile `platform/ios/*` (not `platform/macos/*`) and link the iOS frameworks
