## Why

The full editor ASan run reproduced stack-use-after-scope in ReflectedForm::AssignDefaults. BuildField
bound nested descriptors to an inline Any inside a temporary PropertyField, then moved that field
into a vector. Vector growth and the later field sort could relocate the same backing again. Fixing
parent pointers afterward did not repair the descriptors' object addresses.

## What Changes

- Sort reflected member metadata before constructing any field backing.
- Reserve the final section/child vector capacity and emplace each field directly into its final
  address before creating owned Any values, child descriptors, or parent links.
- Apply the same storage rule recursively to struct and sequence children and default capture.
- Preserve existing editor-kind, sequence read-only, appearance order, and edit/history behavior.
- Add core regressions for small inline structs, multiple nested levels, sibling ordering/capacity,
  multilevel sequences, defaults, and undoable edits/reset.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `editor-reflected-form`: nested field descriptors and parents remain bound to live final storage
  throughout one built tree's lifetime.

## Impact

EditorCore ReflectedForm construction and core tests only. There is no dependency, asset schema,
render/UI-toolkit, or command-service change. View code may still change expansion/presentation
state; structural mutation remains owned by Rebuild. Existing binding-generation and active-edit
reentrancy repairs remain intact. Commit/archive require user confirmation.
