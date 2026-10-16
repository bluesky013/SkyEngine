# ui-editor Specification

## Purpose

Define the headless UI document editing model: mutating the element tree, serializing to the UI document
format, and undo/redo. A visual shell is a separate concern that depends on the render pass.
## Requirements
### Requirement: Document editing operations
The editor SHALL support finding elements by name and adding, removing, renaming, and reordering elements,
using the element-type registry to create new elements.

#### Scenario: Add child
- **WHEN** a child of a registered type is added under a parent
- **THEN** it SHALL appear in the parent's children

#### Scenario: Remove by name
- **WHEN** an element is removed by name
- **THEN** it SHALL no longer be found in the tree

#### Scenario: Reorder siblings
- **WHEN** an element is moved up or down among its siblings
- **THEN** its position in the child order SHALL change by one

### Requirement: Serialization round-trip
The editor SHALL serialize the tree to the UI document format and load it back without losing the element
hierarchy, type, or name.

#### Scenario: Round-trip preserves structure
- **WHEN** a tree is serialized and then deserialized
- **THEN** the reloaded tree SHALL have the same element types and names

### Requirement: Undo and redo
The editor SHALL snapshot the document before each edit and SHALL restore prior and subsequent states through
undo and redo.

#### Scenario: Undo restores previous state
- **WHEN** an edit is undone
- **THEN** the document SHALL return to the state before the edit

#### Scenario: Redo reapplies
- **WHEN** a redo follows an undo
- **THEN** the edit SHALL be reapplied

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
