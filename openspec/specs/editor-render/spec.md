# editor-render Specification

## Purpose
TBD - created by archiving change editor-renderer. Update Purpose after archive.
## Requirements
### Requirement: Editor renderer initialization from the main window

`EditorRender` SHALL initialize from the editor main window's native handle: it SHALL obtain the Aurora device
(reusing an existing one rather than creating a second) and create a `ClientViewport`/swapchain for that window.
It SHALL accept the RHI `API` to initialize with (defaulting to `API::DEFAULT`), so the editor host can select the
backend.

#### Scenario: Initialize from the window handle
- **WHEN** `EditorRenderer::Init` is called with the editor main window handle and size
- **THEN** an Aurora device SHALL be available and a swapchain-backed viewport SHALL be created for that window

#### Scenario: No second device
- **WHEN** an Aurora device already exists
- **THEN** `EditorRender` SHALL reuse it and SHALL NOT create a second device

#### Scenario: Backend selected by the host
- **WHEN** the editor host passes `--rhi dx12`
- **THEN** `EditorRenderer` SHALL initialize Aurora with `API::DX12`

### Requirement: Clear and present frame loop
`EditorRender` SHALL run a per-frame loop that acquires a backbuffer, clears it, submits, and presents it to the
editor window, tolerating a null backbuffer or zero extent by skipping the frame. It SHALL check the surface
extent BEFORE acquiring so a zero-size surface does not acquire an image it cannot present, and SHALL release an
acquired image that turns out unusable instead of leaving it unreleased. It SHALL react to window lifecycle
events so it never acquires a window that is closed: a closed preview window's viewport SHALL be dropped.

#### Scenario: Present a cleared frame
- **WHEN** a frame is rendered with a valid backbuffer
- **THEN** the backbuffer SHALL be cleared to the configured color and presented to the editor window

#### Scenario: Skip invalid frames
- **WHEN** the backbuffer is null or the extent is zero (for example while minimized or out of date)
- **THEN** the loop SHALL skip rendering that frame without crashing

#### Scenario: Zero extent does not acquire
- **WHEN** the surface extent is zero at the start of a frame
- **THEN** the loop SHALL skip acquiring a backbuffer for that frame

#### Scenario: Unusable acquired image is not leaked
- **WHEN** an image was acquired but no backbuffer is available
- **THEN** the loop SHALL release the acquired image before returning

#### Scenario: Closed preview window is not acquired
- **WHEN** the standalone preview window receives a close request
- **THEN** the renderer SHALL drop that window's viewport and SHALL NOT acquire it again

### Requirement: GUI pipeline over sky::ui draw data
`EditorRender` SHALL provide a GUI pipeline that consumes `sky::ui` `UIDrawData` and draws it with batching and
clipping (alpha-blended, depth off), covering solid quads and images at minimum.

#### Scenario: Draw a quad from draw data
- **WHEN** a minimal `sky::ui` document produces draw data containing a quad
- **THEN** the GUI pipeline SHALL draw that quad visibly over the cleared frame

#### Scenario: Batch and clip
- **WHEN** draw data contains multiple elements with a clip rect
- **THEN** the pipeline SHALL batch compatible draws and SHALL restrict drawing to the clip region

### Requirement: Independence from the engine scene pipeline
`EditorRender` SHALL be independent of the engine scene pipeline: it SHALL NOT require `RenderScene` or the engine
top-level renderer to draw the editor UI.

#### Scenario: UI without the scene pipeline
- **WHEN** the editor starts and draws its UI
- **THEN** the UI SHALL render without invoking the engine scene renderer

### Requirement: Render host and module dependencies
The RHI-linked render host SHALL be a target separate from the RHI-free sandbox shell, and the RHI modules it
uses SHALL be registered in the module deploy dependencies.

#### Scenario: Shell stays RHI-free
- **WHEN** the sandbox shell target is built
- **THEN** it SHALL NOT link the RHI, and the render host SHALL link `EditorRender`

### Requirement: Shell-driven GUI content

`EditorRender` SHALL draw the editor shell's `sky::ui` draw data each frame (`UIContext` layout + paint into the
GUI pipeline) and SHALL NOT contain hardcoded mock UI (placeholder rectangles or labels).

#### Scenario: Drawn from the shell context
- **WHEN** a frame is rendered
- **THEN** the GUI content SHALL come from the shell's `UIContext` (`Layout()` + `Paint()`) and its draw data

#### Scenario: Layout changes are reflected
- **WHEN** the layout model or panel set changes
- **THEN** the rendered content SHALL change accordingly, with no code change in `EditorRender`

