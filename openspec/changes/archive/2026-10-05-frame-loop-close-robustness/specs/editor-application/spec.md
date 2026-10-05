## MODIFIED Requirements

### Requirement: Toolkit-independent module loading and frame driver
The application SHALL load modules from configuration and drive a per-frame tick that is independent of any UI
toolkit. When a quit has been requested (for example the main window closed), the loop SHALL stop before ticking
modules so no frame is rendered against a window that is being destroyed.

#### Scenario: Frame tick driven each loop
- **WHEN** the application main loop runs
- **THEN** it SHALL tick loaded modules each frame regardless of the UI framework

#### Scenario: Quit stops the frame before ticking
- **WHEN** a quit is requested during event pumping (for example the main window closed)
- **THEN** the loop SHALL skip the module tick/rendering for that frame and exit
