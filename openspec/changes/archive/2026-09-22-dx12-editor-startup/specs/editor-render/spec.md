## MODIFIED Requirements

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
