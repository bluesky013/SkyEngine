## Context

A committed A→B followed by uncommitted preview C must cancel to B before Undo restores A. The old Shell action path instead traverses history first, allowing the picker to restore B later. Correctness must be owned by the shared command service boundary rather than a particular input widget.

## Decisions

### Prepare at the service boundary

CommandService offers AddBeforeHistoryCallback/RemoveBeforeHistoryCallback using opaque callback ids. Undo and Redo dispatch preparation before inspecting or modifying their stacks, including when the relevant stack is empty. This allows direct calls to end a preview even when no command can be traversed. Execute/Clear are not history traversal and do not dispatch preparation.

Callbacks execute in registration order from a snapshot. Removed registrations are skipped before invocation; additions take effect next time. A callback may remove another observer, destroy a form, or clear history. Stack emptiness is checked after callbacks. Recursive Undo/Redo, including attempts from the subsequent change notification, return false until the outer traversal completes. A scope guard restores this state on exit. The service must outlive its observers and active calls. Clear advances a history generation; Execute/Undo/Redo keep a generation snapshot around command dispatch and do not push the old command into a cleared/replaced session after a synchronous command callback. Undo/Redo still return true when the command ran but its history was cleared. New-session commands created by that callback remain untouched.

### Bind forms to their injected history

Each ReflectedFormView subscribes using the same borrowed CommandService it uses for edits, with its existing weak lifetime guard. It cancels interactions while a valid binding still exists and removes the subscription on destruction. Invalidated models must already have been abandoned/unbound according to T03; history preparation cannot recover an invalid raw model.

The local key path calls Undo/Redo without another reset; key-down history is recognized before inline text dispatch, avoiding duplicate cancellation and key-up traversal. The local key handler also checks a weak form lifetime after service dispatch before refreshing, because a preparation/change observer may destroy the form. Shell actions, menus, toolbars, and direct callers acquire the same ordering without introducing widget or asset knowledge into EditorCore.

### Keep persistence in the asset domain

The hook does not make raw command targets stable or persist cook settings. T04 provides asset-specific history and persistence. Sequence mutations use ReflectedForm's command factory path so they participate in that domain integration; this is separate from the service's preview ordering.

## Validation

Service tests cover final values and cursor, empty stacks, recursive attempts, observer add/remove, and history clearing during preparation/Do/Undo/Redo, including a replacement session with its own commands. Widget tests cover destruction from a preparation observer and empty-stack live preview cancellation. Shell tests route actual accelerator dispatch and shared builder menu/toolbar callbacks through InvokeAction, plus direct external-service traversal, checking that subsequent Escape/mouse-up/synchronization do not overwrite the restored value. Existing widget event hit-testing remains in its own regression suite.

Run the relevant service/widget/shell regressions under the regular build and ASan; record actual asset integration and platform limitations in tasks.md.
