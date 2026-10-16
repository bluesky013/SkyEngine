## ADDED Requirements

### Requirement: Headless document editor is separate from runtime
The headless UI document editor SHALL live in `engine/ui/editor` as target `UI.Editor`, retaining its `ui/editor/...` public includes and existing editing, serialization, and undo/redo behavior. `UI.Editor` SHALL depend on the runtime toolkit; the runtime `UI` target SHALL NOT include or link document-editor implementation. Document-editor tests SHALL link `UI.Editor` in a separate `UIEditorTest` target.

#### Scenario: Runtime builds without document editing
- **WHEN** a runtime application links only `UI`
- **THEN** its dependency graph SHALL NOT include `UI.Editor` or its implementation

#### Scenario: Document editor links explicitly
- **WHEN** a caller links `UI.Editor` and includes `ui/editor/UIDocumentEditor.h`
- **THEN** headless document editing, serialization round-trip, and undo/redo SHALL remain available

#### Scenario: Tests preserve the runtime boundary
- **WHEN** UI test targets are configured
- **THEN** document-editor tests SHALL belong to `UIEditorTest`, and runtime core tests SHALL NOT require `UI.Editor`
