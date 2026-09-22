# editor-preview Specification

## Purpose
TBD - created by archiving change editor-renderer. Update Purpose after archive.
## Requirements
### Requirement: Viewport owns its presentation
A viewport SHALL maintain its own `presentation`, choosing between `TEXTURE` (render to an offscreen content
target composited by the editor UI) and `WINDOW` (render to its own native window + swapchain). The content target
SHALL be independent of the presentation and SHALL be preserved across a presentation change.

#### Scenario: Texture presentation
- **WHEN** a viewport uses `TEXTURE` presentation
- **THEN** its content target SHALL be composited as a `sky::ui` image element in the editor frame, with no
  separate OS window

#### Scenario: Window presentation
- **WHEN** a viewport uses `WINDOW` presentation
- **THEN** a `NativeWindow` and a `ClientViewport`/swapchain SHALL be created for it and its content target SHALL
  be presented there

#### Scenario: Switch presentation at runtime
- **WHEN** a viewport switches presentation at runtime
- **THEN** the window + swapchain SHALL be created or destroyed while the same content target continues to be used

### Requirement: Multiple viewports
The editor SHALL support multiple viewports (the main viewport plus previews) at once, each with its own content
target and presentation.

#### Scenario: Several previews
- **WHEN** the editor creates more than one preview viewport
- **THEN** each SHALL have its own content target and independent presentation state

#### Scenario: Destroy a viewport
- **WHEN** a preview viewport is closed
- **THEN** its content target and any window/swapchain SHALL be released without affecting other viewports

#### Scenario: Per-viewport resize
- **WHEN** a `WINDOW` viewport is resized
- **THEN** only its own swapchain SHALL be rebuilt

### Requirement: Single device for all viewports
All viewport content targets and swapchains SHALL use the one Aurora device; the editor SHALL NOT create a second
device for viewports.

#### Scenario: No extra device
- **WHEN** any number of viewports (TEXTURE or WINDOW) is created
- **THEN** they SHALL all use the existing Aurora device

### Requirement: Viewport content until the scene renderer
Until the engine scene renderer is available, a viewport SHALL be able to present placeholder content; the
presentation mechanism SHALL work regardless of the content source.

#### Scenario: Placeholder content
- **WHEN** the engine scene renderer is not yet available
- **THEN** a viewport SHALL still present placeholder content (for example a clear color / test pattern)

