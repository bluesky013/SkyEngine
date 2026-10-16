## ADDED Requirements

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
