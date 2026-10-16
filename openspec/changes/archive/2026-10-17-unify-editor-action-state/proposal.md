## Why

Toolbar enabled state, menu state and shortcut dispatch currently read action metadata differently. Menus can execute disabled actions and neither chrome surface forwards dynamic visibility or checked state. Evaluating registry-owned callbacks can invalidate the same registration before dispatch.

## What Changes

- Append visible, checkable and checked state to the backend-independent action contract.
- Copy registration snapshots with generation and active scope, resolve predicates safely and centrally validate invocation.
- Forward the same state through menus, toolbars and accelerators. Hidden entries occupy no geometry; disabled entries remain visible and consume their shortcut without running a handler.
- Refresh Shell and Viewer chrome in place, preserving open menus. Rebuild entries only when the registry or active scope changes.
- Guard widget refresh against recursive callbacks, replacement and destruction, with CPU and ASan regression coverage.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- `editor-command-registry`: consistent dynamic state and safe snapshot dispatch across editor chrome.

## Impact

EditorCore action registry, EditorWidgets action builder/menu/toolbar, EditorShell and AssetViewer. RDG, RHI, renderer modules, module action registration and scope ownership architecture are excluded. No commit or archive is included.
