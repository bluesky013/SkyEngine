# Delta: native-platform-backend

## ADDED Requirements

### Requirement: iOS (UIKit) window backend
The framework SHALL provide an iOS window/event backend based on UIKit, behind the existing
`NativeWindow`/`PlatformBase` interfaces (no interface change). The content view SHALL be backed by a
`CAMetalLayer` and `GetNativeHandle()` SHALL return that layer (same contract as macOS). The event loop SHALL be
driven by `UIApplicationMain`, not `NSApplication`.

#### Scenario: iOS surface handle
- **WHEN** an `IOSWindow` is created
- **THEN** its view's backing layer SHALL be a `CAMetalLayer`, and `GetNativeHandle()` SHALL return it so the RHI can create a Metal surface

#### Scenario: UIKit event loop
- **WHEN** the app runs on iOS
- **THEN** `Platform::PollEvent` SHALL drain UIKit events with `UIApplicationMain` owning the outer loop

#### Scenario: Touch and soft keyboard
- **WHEN** the user touches the screen or uses the soft keyboard
- **THEN** the backend SHALL emit engine mouse/touch and keyboard (`OnKeyDown/Up`, `OnTextInput`) events

#### Scenario: DPI scale
- **WHEN** the display scale differs (e.g. @2x/@3x)
- **THEN** `GetDpiScale()` SHALL report the current `UIScreen` scale
