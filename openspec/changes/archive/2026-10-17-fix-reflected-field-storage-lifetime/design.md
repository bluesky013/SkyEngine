## Context

ASan stopped EditorWidgetsTest's existing NestedExpansionAndResizeUseArrangedRowsBeforeAnyPaintRefresh
test in ToggleObject::enabled's getter, called by AssignDefaults. The address belonged to Rebuild's
out-of-scope stack PropertyField. Any uses inline storage for types at or below its block size, so
moving the owning PropertyField changes the address of the reflected value. Larger heap-backed Any
values hid this failure in earlier tests.

The old builder also moved child fields during vector growth and stable_sort. FixParents repaired
the ownership links only; PropertyDescriptor still retained the original object pointer. A fix that
merely reserves the vectors but sorts constructed fields would leave the same bug.

## Goals / Non-Goals

Keep every built field's backing and parent address stable until Rebuild discards the tree. Cover
top-level siblings, nested inline structs, sequences inside nested structs, scratch/default builds,
and normal edit/reset propagation.

Do not redesign Any, PropertyDescriptor, reflected sequence editing, field ordering policy,
CommandService, or ReflectedForm's active-edit/generation reentrancy logic. This repair does not
make arbitrary external structural mutation of the public view model safe.

## Decisions

### Order metadata before allocating descriptor-backed values

Gather pointers to the immutable reflected member metadata and stable-sort those pointers using the
same existing MemberAppearance order. Keep existing top-level visibility and nested member behavior.
The order is therefore known before a single descriptor is bound to owned storage. Appearance
labels/order flags are still applied to the resulting field as before.

### Build fields in final reserved storage

Rebuild creates the section in its owning vector, reserves its full known field count, then emplaces
each PropertyField and calls BuildField on that final object. Struct fields first capture their value
in the final ownedValue, reserve their ordered child count, and construct child descriptors against
that live Any. Sequence fields reserve their complete element count before creating child fields.
Each child receives its final parent pointer before recursion. There is no post-build field sort,
temporary PropertyField move, or later parent-repair pass.

AssignDefaults runs after construction over these stable nodes. The scratch default form uses the
same builder; only independent Any default values escape that scratch lifetime, never descriptors
or field pointers. A later Rebuild deliberately invalidates the old tree, governed by the existing
binding/token contract.

Sequence element descriptors retain the existing index-based, read-only contract. They resolve
through their stable containing descriptor and copy an element before the temporary getter value is
destroyed. The repair does not add unsupported sequence-of-struct editing or alter command targets.

### Reserve structural mutation for the form builder

PropertyField's public header explains that consumers may change expansion/presentation state but
must not move/reorder descriptor-backed fields or mutate the tree structure. Rebuild owns structural
changes. This matches current consumers and avoids adding a new stable-address container/API solely
for a construction defect.

## Risks / Trade-offs

- A future builder must not append beyond a reserved count or sort completed fields. Metadata-first
  construction and small-inline multi-sibling ASan regressions make that invariant visible.
- Default Any values still copy independently; this repair addresses descriptor backing lifetime,
  not semantic equality of aggregate/container defaults.
- Existing active-edit and generation behavior is intentionally untouched; integration still runs
  the reentrancy/binding regressions alongside the storage tests.

## Validation Strategy

- Compile-time assertions keep regression leaf/branch sizes below Any's inline block size.
- Seven top-level fields and nested sibling ordering exercise the former temporary, growth, and
  sort relocation paths; verify values, baseline values, presentation order, and every parent link.
- Edit/reset a deeply nested inline leaf and verify correct root writeback, unaffected siblings,
  Undo, and rebuilt descriptor reads.
- Build a three-level struct containing a 23-element sequence, read every indexed child, capture
  an explicit nested baseline, and verify Add/Remove plus Undo.
- Run existing nested widget test and all new core tests under ASan. Parent coordinates normal and
  ASan builds; this subtask uses syntax checks without writing the shared build tree.
