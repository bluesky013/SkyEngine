## Why

The form-local Undo/Redo key path cancels a live color preview before traversing history, but Shell accelerators, shared menus/toolbars, and direct service calls bypass that cancellation. A later Escape or pointer release can overwrite the value restored by Undo while leaving the history cursor advanced.

## What Changes

- Add an ordered, removable, UI-independent before-history subscription boundary to CommandService.
- Cancel interactions on the exact command service injected into each form before Undo/Redo, including empty-history attempts.
- Reject recursive history traversal and recheck history after preparation callbacks; safely skip removed observers.
- Keep form-local key handling on the same service boundary and run key-down history before inline edit dispatch.
- Add service, widget-lifetime, and real Shell accelerator/shared menu/toolbar callback regressions.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `editor-core`: deterministic preparation before history traversal with observer lifetime and reentrancy rules.
- `editor-reflected-form`: live interactions end before all history entry points, including injected document histories.

## Impact

- CommandService, ReflectedFormView, editor service/widget/shell tests.
- No UI dependency in EditorCore and no asset persistence policy in CommandService; T04 owns that separate domain contract.
