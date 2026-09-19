## Context

Builds on `UIElement`, `UIElementRegistry`, and `UIDocumentLoader`. The visual shell depends on `ui-render`.

## Decisions

### 1. Type names and ordering
`UIElement::GetTypeName` returns the document type key (overridden by widgets) so the editor can serialize
without a reverse registry lookup. `UIElement::MoveChild` reorders siblings for up/down operations.

### 2. Editor operations
`UIDocumentEditor` owns a root `Panel` and a default registry. `AddChild(parentName, type)` creates through the
registry; `Remove`/`Rename`/`MoveUp`/`MoveDown` operate by name. Empty parent name means the root.

### 3. Serialization and history
`Serialize` writes the document format (`{"root": {...}}`) with type/name/visible/enabled/style/children;
`Deserialize` reuses `UIDocumentLoader`. Undo/redo snapshot the JSON before each edit; undo pushes the current
state to redo and restores the snapshot, redo is symmetric.

## Risks / Trade-offs

- **JSON snapshots** are simple but not diff-based; fine for editor-scale trees.
- **No property editing yet** -> only structural edits and name/visibility; property setters can be added on
  top of `UIElement::SetProperty`.
