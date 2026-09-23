## ADDED Requirements

### Requirement: Shell-driven GUI content

`EditorRender` SHALL draw the editor shell's `sky::ui` draw data each frame (`UIContext` layout + paint into the
GUI pipeline) and SHALL NOT contain hardcoded mock UI (placeholder rectangles or labels).

#### Scenario: Drawn from the shell context
- **WHEN** a frame is rendered
- **THEN** the GUI content SHALL come from the shell's `UIContext` (`Layout()` + `Paint()`) and its draw data

#### Scenario: Layout changes are reflected
- **WHEN** the layout model or panel set changes
- **THEN** the rendered content SHALL change accordingly, with no code change in `EditorRender`
