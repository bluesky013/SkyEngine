# Delta: native-platform-backend

## ADDED Requirements

### Requirement: Window DPI scale

`NativeWindow::GetDpiScale()` SHALL return the window's device-pixels-per-point ratio and SHALL reflect
the display the window currently occupies (Windows: per-monitor DPI; macOS: the window's
`backingScaleFactor`), so consumers such as the sandbox editor viewport receive the correct scale.

#### Scenario: Retina window reports 2.0

- **WHEN** a `NativeWindow` is created on a 2x display on macOS
- **THEN** `GetDpiScale()` SHALL return `2.0f` (not the `1.0f` default)

#### Scenario: Scale follows the display

- **WHEN** the window moves to a display with a different backing scale
- **THEN** `GetDpiScale()` SHALL subsequently report the new scale without recreating the window
