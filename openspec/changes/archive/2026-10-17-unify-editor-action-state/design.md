## Context

A state predicate is user code: it can remove, replace or clear registrations, change scope, or trigger host rebuilding. Holding a registry pointer or a widget item reference across that callback is unsafe. The existing scope stack and weak Shell owner signals remain in force.

## Decisions

1. An EditorActionSnapshot copies the action, its monotonically increasing registration generation and the active scope at resolution. Add/Remove/Clear change the registry revision; replacement receives a new generation even under the same key. Existing aggregate registration order is preserved by appending fields.
2. ResolveState copies its input before invoking predicates, checks registration/scope validity between callbacks, and stops when hidden. Invoke resolves state again and runs a copied handler only while the exact registration remains valid. Disabled actions are consumed; stale/hidden actions are rejected. Existing active-scope lookup with global fallback is preserved for direct id lookup; chrome snapshots from a different active scope are rejected.
3. Builder items retain an exact registration snapshot. Their host invoker runs only after snapshot validation, so a retained item cannot invoke a same-id replacement or another active scope. Accelerator matching skips hidden/stale entries; disabled shortcuts retain the existing consumed-without-handler behavior.
4. Menu/toolbar entries retain state resolvers. Hidden entries are excluded from paint, hit testing and geometry; disabled entries use theme disabled colors; checked menu items use the shared checkbox painter and checked toolbar items use the shared UiSkin tool-item painter.
5. Hosts update dynamic state in place before layout/input. Registry revision or active-scope changes replace only chrome entries, retaining the open top-level menu by identity. Normal state changes do not rebuild widget trees or erase open popup state. Each refresh copies callback-owned item data, checks weak widget lifetime and item generation, and bounds recursive refresh.

## Non-goals

Per-window scope tokens, scoped registration ownership, global fallback redesign, plugin unload protocols, module action migration, RDG/RHI/render resources and GPU validation are separate tasks. The work is verifiable using CPU draw data and AddressSanitizer.

## Validation

Exercise shared enabled/checked/visible behavior through menu, toolbar, shortcuts and direct dispatch; stale registration replacement/removal and scope changes; predicate mutation and self-unregistering handlers; dynamic refresh while a popup remains open; hidden geometry compression; disabled/checked CPU draw output; and reentrant widget replacement/destruction/refresh. Build only the existing Core/Widgets/Shell test dependency closure; do not build renderer or runtime modules.
