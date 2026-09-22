## MODIFIED Requirements

### Requirement: Aurora-backed viewport composited into the main window
The editor viewport SHALL render through Aurora and SHALL present according to its own `presentation`: `TEXTURE`
(render to an offscreen target composited by the editor UI, without creating a separate OS window) or `WINDOW`
(its own native window + swapchain). The viewport SHALL own its presentation and SHALL be able to change it while
preserving its content target. The default main viewport SHALL use `TEXTURE`.

#### Scenario: Default texture presentation
- **WHEN** a frame is rendered with the main viewport open
- **THEN** the viewport output SHALL be composited as a UI image in the main window's frame, without a separate
  child native window

#### Scenario: Window presentation
- **WHEN** a viewport uses `WINDOW` presentation
- **THEN** it SHALL create a native window and a `ClientViewport`/swapchain and present there

#### Scenario: Switch presentation
- **WHEN** a viewport switches between `TEXTURE` and `WINDOW`
- **THEN** the window/swapchain SHALL be created or destroyed while the content target is preserved
