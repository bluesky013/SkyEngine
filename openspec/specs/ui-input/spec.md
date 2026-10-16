# ui-input Specification

## Purpose
TBD - created by archiving change ui-input. Update Purpose after archive.
## Requirements
### Requirement: Multi-pointer capture
Pointer events SHALL carry a pointer id, and the router SHALL track capture per pointer so that each pointer's move/release is delivered to the element it pressed, independently of other pointers. Captured delivery SHALL map the rendered-surface pointer through the target's current inverse accumulated transform. When captured geometry is non-invertible, the router SHALL clear that pointer's capture and send coordinate-free pointer cancellation rather than deliver undefined coordinates or leave the gesture captured.

#### Scenario: Independent captures
- **WHEN** two pointers press two different elements and then move
- **THEN** each element SHALL receive only its own pointer's events

#### Scenario: Release clears one capture
- **WHEN** a captured pointer is released
- **THEN** only that pointer's capture SHALL be cleared

#### Scenario: Captured geometry changes
- **WHEN** a captured element's invertible transform changes before a subsequent move or release
- **THEN** that event SHALL be mapped using its current geometry rather than the transform at the original press

#### Scenario: Capture becomes non-invertible
- **WHEN** a captured target's accumulated transform becomes singular
- **THEN** the router SHALL clear that pointer's capture, notify cancellation without x/y, and SHALL NOT invoke its ordinary pointer handler with undefined coordinates

### Requirement: Focus on press
A pointer press SHALL focus the nearest focusable ancestor of the hit element, so a text field begins
receiving input when clicked.

#### Scenario: Click focuses text field
- **WHEN** a press hits a focusable element
- **THEN** that element SHALL become focused and receive subsequent key and text events

### Requirement: Text input routing
Committed text input SHALL be delivered to the focused element and bubble to ancestors while unhandled.

#### Scenario: Text goes to focus
- **WHEN** text input is dispatched while an element is focused
- **THEN** the focused element SHALL receive an `OnTextInput` call

### Requirement: Edit box
An `EditBox` SHALL be a focusable text field that inserts committed text at its caret, deletes the character
before the caret on backspace, and moves the caret on left/right key input.

#### Scenario: Typing inserts text
- **WHEN** text is committed while an edit box is focused
- **THEN** the text SHALL be inserted at the caret and the caret SHALL advance

#### Scenario: Backspace deletes
- **WHEN** backspace is received
- **THEN** the character before the caret SHALL be removed

#### Scenario: Caret moves
- **WHEN** a left or right key is received
- **THEN** the caret SHALL move within the text bounds

### Requirement: Per-receiver pointer coordinates
UIEventRouter SHALL map pointer x/y separately for every target, bubbling ancestor, and hover enter/leave receiver using that receiver's inverse accumulated geometry. Handler x/y SHALL remain untransformed absolute layout-surface coordinates without subtracting the receiver's bounds origin. The original rendered-surface point SHALL be preserved for deriving each receiver's event copy, and pointer id, action, button, and wheel data SHALL be preserved.

#### Scenario: Transformed button receives layout coordinates
- **WHEN** a pointer presses and releases inside the painted bounds of a transformed Button
- **THEN** its handler SHALL receive points in its original absolute layout bounds and SHALL apply its existing inside/outside click behavior

#### Scenario: Bubbling remaps for each ancestor
- **WHEN** an event bubbles from a transformed child to an ancestor with different accumulated geometry
- **THEN** the ancestor SHALL receive coordinates mapped from the original surface point through its own inverse rather than a copy of the child's mapped coordinates

#### Scenario: Hover transitions map independently
- **WHEN** the hovered element changes between differently transformed elements
- **THEN** leave and enter callbacks SHALL each receive coordinates appropriate to their respective receiver

### Requirement: Direct child routing follows relative geometry
Direct ChildRouting SHALL accept pointer x/y in the calling parent's layout coordinates and SHALL map them with each child's local inverse before its hit-test and delivery. Uncaptured target selection SHALL use reverse stable paint/z order and the conservative rendered-surface AABBs of ancestors that declare ClipsChildren, obtained through geometry including the configured surface base. The helper SHALL NOT infer a viewport from a detached root's GetBounds; its caller SHALL enforce viewport boundaries through its host or UIEventRouter, which owns the fixed context surface clip. Captured child input SHALL use the same relative coordinate mapping without requiring a new clipped hit-test. Direct routing SHALL NOT apply the full accumulated inverse again to an already parent-mapped point or dispatch through a non-invertible surface base or accumulated child path. An unmappable direct capture SHALL be cleared before coordinate-free cancellation is sent to the captured child.

#### Scenario: Direct route maps one transform step
- **WHEN** a parent's handler forwards an already mapped pointer event to a transformed child
- **THEN** the child SHALL receive the result of its local inverse only, with no duplicate ancestor/surface conversion

#### Scenario: Direct overlap follows paint order
- **WHEN** overlapping children differ in z or equal-z insertion order
- **THEN** direct routing SHALL consider them in reverse of their stable paint order

#### Scenario: Direct captured child remains mapped
- **WHEN** a direct-route child is captured and receives later move/release outside its bounds
- **THEN** the captured input SHALL use its current local inverse without requiring a new clipped hit-test, and a non-invertible accumulated path SHALL prevent undefined dispatch

#### Scenario: Direct route respects surface-base clipping
- **WHEN** a direct child route receives a parent-layout point under a configured surface transform
- **THEN** uncaptured target selection SHALL check the corresponding rendered-surface point against ClipsChildren ancestor AABBs using the parent's default geometry

#### Scenario: Detached bounds do not invent a viewport
- **WHEN** a detached root does not clip children and a child extends beyond that root's bounds
- **THEN** direct routing SHALL NOT reject the child solely because of those root bounds, and the caller SHALL remain responsible for any actual viewport restriction

### Requirement: Unusable capture sends coordinate-free cancellation
UIElement SHALL provide OnPointerCancel(pointerId) with a default no-op implementation. If a router's captured target cannot map a pointer due to non-invertible geometry, the router SHALL clear that pointer's capture before notifying the target and its ancestors through OnPointerCancel. Direct-child capture owners SHALL clear and notify an unmappable captured child, and slot containers SHALL be able to forward cancellation through their direct capture. Cancellation SHALL require no mapped x/y, SHALL NOT produce a Button click, and SHALL clear active pressed/drag/preview state in the affected Button or reflected form. This requirement SHALL be limited to invalid geometry cleanup and SHALL NOT change focus policy or other pointers' captures.

#### Scenario: Singular release does not leave a Button pressed
- **WHEN** a Button is captured on press and its accumulated geometry becomes singular before release
- **THEN** cancellation SHALL clear its pressed state and capture without emitting a click, allowing a later invertible gesture to work normally

#### Scenario: Cancellation traverses target and ancestors
- **WHEN** a router cannot map a captured pointer because an element, ancestor, or configured surface transform is singular
- **THEN** it SHALL clear the capture and notify target-to-ancestor cancellation without attempting to construct local coordinates

#### Scenario: Direct capture cancels safely
- **WHEN** a direct-child capture becomes unmappable
- **THEN** its owner SHALL clear the captured reference before notifying that child, including forwarding through a slot container's own direct capture where applicable

#### Scenario: Reflected preview and drag are canceled
- **WHEN** a reflected form has an active drag or live preview and its captured geometry becomes unusable
- **THEN** pointer cancellation SHALL clear that interaction and restore an uncommitted preview without retaining obsolete capture or field references

#### Scenario: Other pointers remain captured
- **WHEN** one pointer's unusable capture is canceled while a different pointer retains valid captured geometry
- **THEN** the second pointer's router capture SHALL remain active

### Requirement: Input state belongs to a live context attachment

Focus, hover and per-pointer capture SHALL reference only elements attached to the router's live
UIContext. Removing or clearing a subtree SHALL clear all input state targeting that subtree while
preserving targets in unrelated subtrees. A later attachment, in the same context or a different
context, SHALL NOT inherit the removed attachment's focus, hover, capture or pressed interaction.
MoveChild sibling reordering SHALL preserve the current attachment and its input state.

#### Scenario: Detach one of two independently captured branches

- **WHEN** two pointers capture elements in different branches and one branch is removed
- **THEN** the removed branch's focus, hover and captures are cleared before it can be destroyed
- **AND** unrelated focus, hover and other-pointer capture remain intact

#### Scenario: Reattach a retained pressed button

- **WHEN** a pressed button is removed, retained by its caller, attached again and receives UP
- **THEN** it does not click from the removed attachment's press
- **AND** a fresh DOWN and UP can establish a new click

#### Scenario: Transfer or reorder an element

- **WHEN** RemoveChild and AddChild transfer an element within or between contexts
- **THEN** old router state is cleared and destination focus/capture require fresh input
- **WHEN** MoveChild only changes sibling order
- **THEN** existing focus, hover and captures remain associated with that live attachment

#### Scenario: Explicit focus uses a foreign or detached target

- **WHEN** SetFocus is given an element outside the router's context or a detached element
- **THEN** that element does not become a focused dispatch target

### Requirement: Detach cancellation is precise and reentrant

Before invoking detach cancellation callbacks, the implementation SHALL clear all affected router
and native direct-child input references. It SHALL call OnPointerCancel on affected actual targets,
deduplicated by target and pointer id within the pending interaction batch; focus and hover SHALL contribute pointer id 0 and captures
SHALL contribute their original pointer ids. Removal SHALL NOT fabricate OnPointerLeave coordinates
or cancel unrelated ancestors/subtrees. Reset SHALL retain silent state-clearing behavior.
Pending cancellation identity SHALL survive routing-state removal until delivery, SHALL NOT keep
the target alive, and SHALL be claimed before user cancellation code runs. Nested detach operations
SHALL settle undelivered old cancellation before returning ownership or destroying their extracted
subtree. An outer cancellation batch SHALL NOT cancel fresh interaction begun after transfer.

#### Scenario: One element is focused, hovered and captured by pointer zero

- **WHEN** that element's subtree is detached
- **THEN** all three references are already cleared when cancellation runs
- **AND** that target receives a single cancellation for pointer zero and no synthetic leave event

#### Scenario: A cancellation callback mutates the tree

- **WHEN** cancellation removes another captured subtree or ClearChildren cancellation adds a child
- **THEN** pending cleanup does not access invalidated nodes or iterators
- **AND** the outer ClearChildren preserves children added after its original collection was extracted

#### Scenario: A cancellation callback transfers another pending pressed target

- **WHEN** an earlier cancellation callback transfers another pending target to a different context
- **THEN** nested removal cancels that target's old pressed state before returning it to the caller
- **AND** fresh input started after transfer survives the remaining old cancellation batch

### Requirement: Dispatch validates callback lifetime and attachment

Pointer, key, text, hover and direct-child routing SHALL revalidate object lifetime and tree
attachment after callbacks before accessing the old target, walking an ancestor or delivering to a
previously computed hit. Detach followed by immediate reattachment SHALL invalidate the old event
path even if the object survives at the same address. Direct-child routing SHALL release capture
before UP callbacks and treat WantsPointerCapture as a callback boundary. Native containers retaining
a single direct capture SHALL preserve it when an event or cancellation belongs to another pointer;
those unrelated events SHALL return unhandled without replacing or releasing the captured target.

#### Scenario: A handler removes its own ancestor

- **WHEN** a pointer, key or text handler removes and destroys its own ancestor while unhandled
- **THEN** bubbling stops without reading the destroyed element or ancestor

#### Scenario: Hover callbacks destroy current or next targets

- **WHEN** enter destroys the current hit or leave destroys the next computed hit
- **THEN** no subsequent enter or pointer delivery uses the destroyed target

#### Scenario: A handler reparents its own element

- **WHEN** a handler removes and reattaches its element during an event
- **THEN** the original event does not bubble through either stale old ancestors or new ancestors

#### Scenario: An unrelated pointer targets a direct container with capture

- **WHEN** pointer 22 has direct capture and the container receives Cancel, UP or DOWN for pointer 11
- **THEN** pointer 11 does not clear, replace or deliver through pointer 22's capture
- **AND** pointer 22 can still release its captured target normally

#### Scenario: A handler destroys its router or context

- **WHEN** pointer, key, text or hover user code destroys the active router or its UIContext
- **THEN** in-progress dispatch stops without reading the destroyed owner or following its old route

### Requirement: Router and context teardown leave no borrowed targets

Every router attached to a context SHALL observe its subtree invalidation. Router destruction SHALL
remove its context registration. Context destruction SHALL invalidate registered routers before tree
release, and a surviving router SHALL be safe and inert until its own destruction.

#### Scenario: Two routers observe one context

- **WHEN** a subtree referenced by multiple routers is detached
- **THEN** each router loses only its affected targets before any cancellation callback runs

#### Scenario: Context is destroyed first

- **WHEN** a context is destroyed while a router survives
- **THEN** router targets become null and hit testing, dispatch and Reset are safe without context access

#### Scenario: Router is destroyed first

- **WHEN** a router is destroyed and its former context later removes children
- **THEN** no notification calls into the destroyed router
