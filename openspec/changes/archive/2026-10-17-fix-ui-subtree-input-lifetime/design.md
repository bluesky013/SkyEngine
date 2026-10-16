## Context

UIContext owns one element tree, while one or more UIEventRouters borrow that context and retain raw
element targets. UIElement owns children through unique_ptr. The tree has no automatic router detach
notification, and the old Bubble helpers follow element->GetParent after invoking arbitrary user code.
UpdateHover can likewise lose either the old hover target or the precomputed next hit in a leave/enter
callback. A RemoveChild return value can keep an element alive, so object lifetime alone cannot
distinguish an old interaction from the same object attached again elsewhere.

StackPanel, DockPanel, GridPanel and Overlay also support direct child pointer routing and retain a
captured child separately. Clearing only router state would leave their retained press/drag active.

## Decisions

### Separate object lifetime from tree attachment

Track element lifetime independently from each tree attachment. A detach invalidates the entire old
subtree attachment before calling user cancellation code. AddChild establishes membership under the
new parent/context. Remove followed by Add, including the same parent and context, deliberately clears
the old interaction. MoveChild changes sibling ordering only, so its existing attachment and input
state survive. Objects may remain owned by the RemoveChild caller while their old route is invalid.

SetFocus accepts only elements belonging to the router's live context; detached or foreign elements
cannot become dispatch targets. This membership check is not an added focusability policy.

### Extract ownership and invalidate state before callbacks

RemoveChild extracts the child's ownership and erases the old child entry before notification.
ClearChildren extracts the original child collection before detaching it, so a cancellation callback
can remove another old target or add a replacement child without invalidating an iterator or causing
the outer clear to erase the replacement. Cancellation keeps the removed objects alive until their
owning operation completes, and callback iteration checks liveness before each remaining target.

The context maintains its attached routers. Collect and clear affected focus, hover, per-pointer
capture and native direct-child capture before invoking cancellation callbacks. State in unrelated
subtrees remains unchanged. Router state contributes actual affected targets, rather than bubbling
cancellation into ancestors outside the removed subtree. Direct container capture contributes its
own captured target and original pointer id through the same cancellation collection.

Focus and hover contribute pointer id 0; capture contributes its actual pointer id. The collection
deduplicates each element/pointer pair, including overlap between focus, hover, router capture and
direct-child capture. Use OnPointerCancel rather than fabricating coordinates for OnPointerLeave.
Reset retains its existing silent state-clearing contract.

Cancellation remains pending interaction state until delivered. If an earlier cancellation callback
transfers another pending target, that target's old cancellation must be settled before the fresh
attachment begins. Merely skipping an expired attachment token would preserve the old Button press;
delivering the old cancellation after a fresh DOWN would instead erase the new interaction. The
implementation therefore carries pending cancellation identity across the collection/dispatch
boundary, drains it before reattachment and makes the outer pending entry inert after delivery.

The collection phase queues a shared delivery ticket for each target/pointer pair before
OnChildrenChanged or cancellation callbacks run. Each target keeps only weak references to its
pending tickets, avoiding ownership cycles or keeping removed widgets alive. Duplicate focus,
hover, router and direct-container entries share the same ticket. Dispatch marks the ticket
delivered before entering OnPointerCancel, so recursive cancellation cannot deliver it again.

A nested RemoveChild or ClearChildren collects undelivered tickets already queued on its subtree,
even though outer routing references have been cleared. It associates those tickets with the
subtree's current detached attachment, clears any additional current routes, and delivers them
before returning ownership or destroying the extracted collection. A caller can therefore attach
the returned object elsewhere and start fresh input only after its old press is settled. The outer
batch skips a consumed ticket, dead target or obsolete attachment and cannot cancel a fresh press.
AddChild itself does not create or dispatch synthetic cancellation; valid ownership transfer passes
through the detach operation that settles the pending tickets.

### Validate every callback boundary

Router dispatch captures object and attachment validity before invoking pointer, key, text, enter,
leave or cancellation code. It checks both afterwards before reading the old target, following its
parent, or delivering to a precomputed hit. Detach and immediate reattach invalidate the in-progress
old route even if the same object address remains valid. Destruction of a pending target during a
previous callback makes that pending route inert.

Direct-child routing uses the same liveness and attachment boundaries. Release local capture before
UP delivery, and do not access the container's captured-reference storage after a callback that can
destroy that container. Calls to WantsPointerCapture are also user-code boundaries. Preserve existing
clip, geometry, z-order and event-result behavior for unmodified trees.

Native direct containers retain their existing single-capture policy, but remember the pointer id
that established capture. Events or cancellation from another pointer return unhandled or have no
effect, preserving the original pointer's target until its own release/cancellation. This does not
introduce a new multi-capture direct-container API; the router already supports independent pointers.

### Permit either context/router destruction order

Router destruction unregisters it from the still-live context. Context destruction invalidates all
registered routers before destroying the tree, and a surviving router becomes inert for HitTest,
DispatchPointer, DispatchKey, DispatchText, Reset and destruction. Multiple routers on the same
context all observe subtree invalidation. No callback may leave another router with a stale target.

## Non-goals

Do not change unique_ptr ownership, make arbitrary host code safe after it deletes itself, introduce
shared ownership of all widgets, rewrite global Core Event dispatch, or redesign focusability,
visibility, disabled routing or IME semantics. Renderer/native-window lifetime and all RDG/RHI work
remain excluded while those systems are refactoring. Widget-specific external model lifetime still
uses the existing binding/session contracts.

## Validation

UICoreTest regressions cover exact leaf cancellation, unrelated sibling state and independent pointer
ids, ancestor removal, ClearChildren callback additions, same-context reattachment, cross-context
transfer, sibling reorder, detached/foreign focus rejection, multiple routers and either context/router
destruction order. Destructive pointer/key/text/enter/leave/cancel callbacks exercise stale-target and
old-attachment rejection, including destruction of the router or entire context from inside a
handler or cancellation callback. A real Button verifies DOWN then remove/reattach then UP cannot
click. A cancellation-triggered transfer also verifies that old pending press state is cleared
before a fresh attachment, while fresh input started inside that callback survives the outer batch.
Direct-container regressions cover capture cancellation, unrelated-pointer isolation and
callbacks destroying container/children. Combined direct/router and multi-router targets verify
deduplication before notifying user code.

Run only portable affected CPU suites normally and under AddressSanitizer, preserving the current
RDG/RHI exclusion. Do not run renderer, SandboxModule or full-default builds to verify this change.
Run strict OpenSpec validation and whitespace checks; record actual results before completing tasks.
