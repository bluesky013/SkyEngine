## Why

Preferences binds live page objects to the editor-wide command service and restores opening snapshots with indexed byte copies. Cancel leaves stale global commands, Apply does not establish an isolated history boundary, and a page containing `std::string` or a vector cannot be restored safely by `memcpy`. Reordering or replacing page registrations can restore the wrong object.

## What Changes

- Add a UI-free `PreferencesSession` in EditorCore, with owned value copies of reflected pages and a session-local command service.
- Edit working pages and write reflected writable members to live settings only on Apply/OK. Cancel, Close and destruction discard unapplied copies; reopening reads the applied live values.
- Assign a new registration generation on every registry Add. Preserve stable working object addresses/history across page reorder; clear history before releasing removed/replaced working values, including same-address re-registration.
- Make Preferences bind/unbind/destruction order explicit, defer model transitions during committed widget editing, and guard external callback reentrancy/destruction.
- Add CPU regressions for deep values, cross-page history, dynamic registrations, Apply baselines and observer/callback lifetime paths.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- `editor-preferences`: owned working values, independent session history, registration identity and Apply/Cancel lifecycle.

## Impact

- EditorCore preferences, PreferencesDialog and headless shell tests; existing reflected field controls remain in use.
- **Behavior change:** field edits no longer mutate live settings before Apply/OK. A page type must have safe value-copy semantics. Invalid/non-copyable and duplicate-id pages are excluded from the editable session.
- Persistence/application remains a host callback; generic plugin persistence and native settings application are deferred to T11-C.
- RDG, RHI, renderer/surface/frame code, UI Aurora adaptors, SandboxModule and Core Event are outside this change. No commit or archive is included.
