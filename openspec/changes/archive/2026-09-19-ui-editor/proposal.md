## Why

Authoring UI as data needs an editing model. A visual shell cannot run before the Aurora render pass exists, so
this change delivers the render-independent editor core: tree mutation, serialization, and undo/redo.

## What Changes

- Add `UIDocumentEditor`: find by name, add/remove/rename/reorder elements via the element registry.
- Serialize the tree to the UI document format and load it back (round-trip).
- Snapshot-based undo/redo.
- Add widget type names (`UIElement::GetTypeName`) and sibling reordering for serialization/editing.

**Non-goals**: a visual editor window, drag-drop, property inspectors, and multi-user editing.

## Capabilities

### New Capabilities
- `ui-editor`: headless document editing, serialization, and undo/redo.

## Impact

- `UIElement` (type name, `MoveChild`), new `editor/UIDocumentEditor.{h,cpp}`; tests in `engine/test/ui`.
