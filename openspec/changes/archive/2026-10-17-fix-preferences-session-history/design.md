## Context

The existing dialog owns a reflected form but borrows page objects from a cross-DLL registry. Its default form uses the global command service. Indexed raw byte snapshots are only valid for trivial objects and cannot distinguish registration replacement. A local history alone would still leave raw property command targets invalid after working-page replacement or derived-member destruction.

## Decisions

### Owned working values and explicit Apply

`PreferencesSession` resides in EditorCore and depends only on its existing core/framework services. It copy-constructs each reflected object into an owned `Any`. It never invokes the reflection copy-constructor callback as assignment on an already constructed live object, and never reconstructs a live object in place. Standard owning strings/vectors copy deeply according to their ordinary type semantics. Non-copyable/invalid types and ambiguous duplicate ids are excluded; an owning raw pointer is not made safe by a copy constructor and remains unsupported unless the type supplies correct copy semantics.

Each page value lives in a `unique_ptr` node, including inline Any values. Reordering the node vector cannot relocate the working object referenced by a reflected form or command. Edits, Reset and cross-page Undo/Redo operate on these values through an injected local command service.

Apply gathers owned top-level reflected values, verifies the current registration before every write, and invokes its setter/notification path. Read-only and non-reflected live members remain unchanged. Apply clears the local history before calling the host; subsequent working edits are discarded by Close while the applied live values persist. A new Open discards any older session and starts from current live values. This deliberately replaces immediate live preview of uncommitted Preferences fields. Incomplete field previews/text edits are cancelled before Apply.

Apply excludes const members, `READONLY`/invisible metadata and members without usable getter/setter functions. Actual static-member registrations expose no setter and are excluded. The current reflection factory incorrectly marks ordinary instance members `isStatic`; this change does not trust that flag or repair Framework reflection. Correcting that metadata is a separate follow-up.

Apply is not an atomic transaction for arbitrary setters with external side effects. A setter that closes/replaces the session or invalidates its page stops subsequent writes and suppresses the success callback; already performed setter side effects are the provider's responsibility.

### Registration identity and reconciliation

`PreferencePageRegistry::Add` assigns a monotonically increasing registration number in the existing process-shared Singleton; Clear never resets it. Identity includes stable page id, registration number and reflected type/object identity. Same-id/type/address Clear-and-re-add therefore cannot reuse an old session. Remove and Move support dynamic contributors without exposing mutable registry storage.

Reconciliation preserves still-matching owned nodes and selection by page id. A pure reorder/title update preserves history. Membership or registration changes clear all local history before old nodes are released. The host unbinds the form before reconciliation. Reconciliation reacquires live registry entries after history notifications, because a Clear observer may reallocate that registry. Session generation checks make a synchronously opened replacement session win over an older End/Apply/Synchronize operation.

The registry and reflected type metadata remain borrowed and must outlive the session. This change is not a plugin-unload ownership system.

### Dialog and callback lifetime

PreferencesDialog destroys/unbinds child forms in its destructor body before local command services are destroyed. It retains a local shared session during notification-capable transitions and event dispatch, while independent weak dialog/binding tokens reject access to a destroyed owner. Retaining the session never keeps an invalid field binding alive.

Open, Close and model replacement requested from an active committed form edit are queued until a safe later event/layout boundary. This preserves the widget's current field until its event tail returns. Reset similarly stops further field iteration if a transition is requested or the owner is destroyed.

Apply/OK finish internal state before external callbacks. Callbacks are copied; `onApplied`/`onClosed` may replace page registrations, close, reopen or destroy the dialog. A generation check prevents an old OK close notification from affecting a newly opened session. A bare external `History()` reference remains borrowed: its owner must stay alive for that external service call. The generic CommandService lifetime contract is unchanged.

### Scope

The existing generic reflected fields remain. Category/footer layout migration and generic plugin persistence/native application are T11-B/T11-C. The host currently persists its three built-in pages. RDG/RHI/renderer/surface/frame, Aurora adaptors, SandboxModule and Core Event are untouched.

## Validation

CPU tests cover owned strings/vectors, unreflected-member preservation, Apply followed by Cancel/reopen, global-history isolation, 40 inline-valued pages with cross-page reorder Undo/Redo, removal/type replacement/ABA registration, registry mutation during Clear, reentrant session replacement during a setter notification, actual dialog Close/Apply/OK and callback destruction/reopen, transition notification destruction, Ctrl+Z event destruction, and a Close requested inside a committed edit. Root runs the portable editor targets and ASan; Windows/native/GPU acceptance remains separately queued and is not required to develop this CPU-only change.
