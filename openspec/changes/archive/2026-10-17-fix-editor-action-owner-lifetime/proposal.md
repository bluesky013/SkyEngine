## Why

Full-suite ASan found that EditorShell's registered asset-viewer action predicates retained a raw shell pointer after destruction. A later independent AssetViewer built shared chrome from the process registry and evaluated CanUndo through that expired owner. Action copies retained by other consumers have the same lifetime problem.

## What Changes

- Give each EditorShell a private lifetime signal and invalidate it at the start of destruction.
- Capture that signal weakly in all five asset-viewer action invoke handlers and enabled predicates. Expired-owner actions become disabled/inert before touching the shell or its viewer.
- Keep registrations belonging to a newer shell intact; do not clear the process registry or remove same-ID actions without ownership identity.
- Add regressions that consume retained action copies and build independent viewer chrome after shell destruction, plus two-shell replacement ownership.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- `editor-command-registry`: host-bound actions remain safe when registry entries or snapshots outlive their owner.

## Impact

- EditorShell's action registration/destruction and EditorShellTest lifecycle regressions.
- This is an object-lifetime repair. Per-window scope-token ownership, action metadata forwarding, and module/DLL unload registration ownership remain separate work.
- No commit or archive is included.
