# Editor Preferences & Settings

The Preferences dialog renders and edits editor settings through the **generic reflected form**
(`ReflectedFormView`), not per-type controls. Settings are reflected structs with UI metadata — the
same model Unreal (`UDeveloperSettings`), Godot (`EditorSettings` + `PROPERTY_HINT_*`) and Blender
(RNA) use. See the archived change
`openspec/changes/archive/2026-10-11-editor-preferences-reflected/`.

## Model

- `engine/sandbox/core/include/editor/core/preferences/EditorSettings.h` — one reflected struct per
  page: `GeneralSettings`, `EditorViewSettings`, `RenderingSettings`, plus enums
  (`EditorLanguage`, `EditorTheme`).
- `RegisterEditorSettings(SerializationContext*)` — registers the types and their metadata
  (`LABEL`, `ORDER`, `RANGE_MIN/MAX/STEP`, enum names) through the core `CommonPropertyKey`
  attribute map, which `PropertyEditorRegistry::Resolve` turns into an `EditorControl`.
- `PreferencePageRegistry` (cross-DLL `Singleton`) — the ordered `{ id, title, object, TypeNode }`
  page list. Every `Add` receives a process-wide, monotonically increasing registration identity;
  `Clear` does not reset it. Dynamic is the **page list**; each page's fields are a **fixed reflected type**.
  A page may also supply a pure candidate `validate` callback and a retry-safe `apply` callback.
- `PreferencesSession` — UI-free working objects and a private `CommandService`. Each page is
  copy-constructed into an owned `Any` in a stable heap node. Ordinary value members such as
  `std::string` and `std::vector` keep their copy-constructor ownership semantics; there is no byte snapshot.
- `BuiltinPreferencePages` owns only the exact registration ids of the host's three built-in pages.
  Rebinding/destruction preserves plugins and newer owners; reserved-id collisions return Conflict.

Data and registration live in core (UI-free); the shell only binds.

## Why reflected structs (not a key/value schema)

`ReflectedFormView` is driven by compile-time reflection: `TypeMemberNode` holds raw
getter/setter function pointers that only receive the object pointer, so a member cannot resolve a
**runtime string key**. A runtime key/value schema (the previous `PreferenceRegistry`) therefore
could not be rendered by the reflection form; expressing settings as structs does. This is the
AGENTS "dynamic, schema-driven settings" case — schema-driven at the page level, reflection-driven
at the field level, so the controls come from reflection rather than a hand-written per-type switch.

## Dialog

`PreferencesDialog` keeps its category list and footer (OK / Cancel / Apply / Reset) and hosts a
single `ReflectedFormView` bound to the selected page's object.

- `Open` starts fresh working copies and empty local history. The form edits these copies through
  an explicitly injected local `CommandService`; field editing no longer changes the live registry
  objects before Apply. Switching pages preserves cross-page undo/redo. Preferences keyboard
  shortcuts remain in the modal session, and never traverse the editor-wide document history.
- **OK / Apply** cancel incomplete previews, validate owned candidates, optionally save through an
  injected portable service, then publish guarded reflected writable values and page application
  callbacks. Successful publication reaches a guarded local-history clear; only complete success
  invokes `onApplied()`. Apply leaves the
  dialog open; successful OK closes it first. A failure leaves the current session open for correction
  or retry. Non-reflected and read-only live members are preserved. A legacy caller with no injected
  service may still use the success-only `onApplied()` notification for its old host persistence.
- **Cancel / Esc / Close**, model replacement, and destruction discard unapplied working changes.
  They never roll live objects back or invoke undo on the global history. After Apply, subsequent
  edits can be discarded while the applied live values remain. Reopening begins from those live values.
  A failed operation that already saved/published/applied something has the same discard-only
  boundary: Cancel does not roll back those reported partial effects.
- **Reset** routes each top-level field through `ReflectedForm::ResetToDefault` and the same local
  history; it remains unapplied until Apply/OK.
- Pages match by stable id, reflected type/object identity and registration generation, not by list
  index. Reordering preserves stable working addresses, selected page id and cross-page history.
  Add/remove/re-registration clears history before releasing old working values. A re-added page is
  fresh even if id, type and object address are unchanged. Invalid/non-copyable or duplicate-id pages
  are excluded from the editable session.
- A bound form is cancelled/unbound before working storage is replaced; child forms are destroyed
  before local services. Open/Close/model replacement requested during a committed form edit is
  deferred to the next safe event/layout boundary, after the widget finishes dispatching.
- Applied/closed callbacks are copied before invocation and may close, reopen, replace the registry
  pages or destroy the dialog. An old OK callback does not close a session reopened by `onApplied`.
  Event/transition dispatch retains local session storage while weak dialog/binding tokens reject
  further access to a destroyed host. A caller retaining a bare `History()` reference must keep the
  owning dialog alive for that external service call; this is not shared ownership of arbitrary widgets.

The registry and reflection type metadata remain borrowed and must outlive the session. Settings
types must support safe value copying: owning standard containers are supported; copying an owning
raw pointer does not acquire ownership. Reflection setters/notifications retain their existing host
lifetime contract. Apply is not a transaction across arbitrary setters with external side effects;
if a setter replaces/closes the session or invalidates a registration, further writes stop and no
success callback is emitted.

## Container composition and layout

T11-B (`compose-preferences-container-layout`) replaces the surrounding category/page/footer
rectangle formulas with an owned container tree. The session and generic reflected fields above
retain their behavior.

```text
PreferencesDialog
└─ DockPanel: bounded panel
   ├─ Header leaf: Top, UiMetrics::panelHeaderHeight
   ├─ Footer DockPanel: Bottom
   │  └─ GridPanel: Right, 4 / 2 / 1 columns
   └─ Body DockPanel: Fill
      ├─ Category ScrollView: Left, vertical
      │  └─ VBox: category row leaves
      └─ ReflectedFormView: Fill, stable instance
```

The panel is constrained to the available viewport with themed margins. Each dock extent and
padding is clamped to its available budget before the shared containers arrange children; this
does not change the shared container algorithms or ModalDialog's behavior for other dialogs.
The category column is capped to leave page space. The footer measures its labels against the
theme's button minimum and switches its right-aligned grid between four, two, and one columns
as width decreases. Degenerate viewports may produce empty clipped regions, but no negative
allocations or active hidden targets. Panel dimensions, row/control sizes, spacing and colors come
from UiTheme. The actual header leaf uses the same `panelHeaderHeight` as UiSkin, removing the
old disagreement between the dialog's title offset and its painted panel header.

Category rows stay single-line and use the shared skin/text ellipsis within their allocated clip.
Their vertical ScrollView and the form's existing internal scrolling are independent: a wheel over
categories moves only category rows; a wheel over the page moves only form content. The footer
stays fixed. The form remains attached to the body's Fill slot across page selection and rebinding,
so scrolling/navigation does not replace the form or its injected session history. Its active
popup and pointer capture retain input priority outside the ordinary page viewport. A popup may
paint beyond the modal panel while remaining inside the window; the panel composition does not
add an ancestor clip that truncates it. Normal category rows and content retain their own clips.
Captured form interactions are pointer-specific, and matching cancellation resets the form before
a new pointer session begins.

`PreferencesDialog::SynchronizeLayout()` prepares safe pending transitions, registry/page data,
the form and theme, then configures/arranges the composed tree before paint or input. Geometry
queries use this same synchronization path, including before the first paint and after resize or
same-scale metric/scale changes:

- `PanelRect()` and `ContentRect()` describe the constrained panel and arranged body.
- `CategoryViewportRect()` and `PageViewportRect()` expose the actual navigation and form
  viewports. Use these when routing a region-specific wheel or checking visible containment.
- `CategoryRect(index)` and `ButtonRect(index)` report arranged child bounds. A scrolled row's
  full rectangle may extend outside its viewport; `CategoryAt(x, y)` additionally rejects clipped
  or empty regions. `ButtonAt(x, y)` rejects empty/outside controls. Invalid indices return an
  empty rectangle. Button indices remain 0 OK, 1 Cancel, 2 Apply, and 3 Reset.

These const geometry queries synchronize observable UI state; they are not pure measurement
queries. Hosts should set/arrange the dialog's outer bounds before requesting geometry. Paint,
input and the queries consume the same arranged tree rather than separate row/button formulas.
Panel allocation retains its unpositioned logical size explicitly, including at large absolute
origins; rounded rectangle endpoints are not subtracted to reconstruct that measurement budget.
OnPaint only consumes the prepared composition and does not reconcile pages or create/rearrange
rows. Layout, input, explicit synchronization and geometry queries prepare changed page/theme data.
The existing session transition, weak lifetime checks and child-before-service destruction order
remain necessary while synchronization can invoke observers.

## Portable persistence and application protocol

T11-C (`persist-preference-pages-portably`) adds an opt-in, UI-free `PreferencesPersistence` service
in EditorCore. Portable implementation and normal/ASan verification are complete, with results in
`ui-layout-and-chrome-plan.md` §17. It uses a shared injected `IPreferenceStorage` rather than a
native path. `Read` distinguishes absent storage (`NotFound`) from read failure. Failed or throwing
`Write` must preserve the adapter's previous bytes; an adapter that can partially replace data on
failure does not satisfy this protocol. The owner serializes calls on a store.

The saved envelope remains `{pageId: {classId: UUID, elements: {...}}}`, compatible with existing
page-id files. A checked codec uses Any/reflection metadata without assertion-based archive loading
or blindly executing custom codecs. It handles supported reflected values, reports invalid/unsupported
data explicitly, and preserves provider values for missing pages/fields. Save reads the current
document and merges known fields while retaining unknown root/page/object keys and absent-plugin
pages. Known arrays replace old contents, including replacement with an empty array. Runtime page
registration numbers and addresses are not persisted.
Save explicitly clones candidates with Any::Create before callback-capable storage/getter work.
Owned field reads prefer a nonconst getter when available, retaining support for reflected types
without a const getter and checking validity after each callback.

`PreferencesPersistence::Load(registry)` is a host-controlled operation with no open editing
session. It copies provider values, decodes and validates all candidates, then publishes through
guarded setters/application callbacks. Decode/validation/read failures publish nothing. It does not
discover or silently rebase a currently open dialog's working pages. Encoding includes readable
reflected fields; decoding changes only writable candidate fields, and publication retains the
session's const/no-setter/READONLY/invisible filtering. The unreliable existing `isStatic` flag is
not used as a persistence filter.

`PreferencesSession::ApplyDetailed` orders validation → optional Save → guarded live publication /
page application → guarded history clear and completion check. `Apply()` remains the bool compatibility wrapper.
Read, serialization, validation and failed Write preserve live settings and current working/history
state. A later setter/application/identity failure may occur after durable bytes or live effects;
there is no transaction across arbitrary I/O, setters and callbacks. Application callbacks must
tolerate retry, and Cancel only discards the remaining working session.

`PreferenceResult` carries a code, page/path/message context, and flags for this operation:

- `storageWritten` becomes true only after Write explicitly succeeds.
- `liveWritten` becomes true before entering a live setter.
- `applicationApplied` becomes true before entering a page application callback.

The latter two flags conservatively indicate a potentially effectful stage was entered, including
rejection/throw; they do not prove a value changed or application completed. `Succeeded()` reports
complete success and `HasPartialEffects()` identifies failed operations that crossed these boundaries.

Hosts can inject a shared service through `EditorShell::SetPreferencesPersistence` or
`PreferencesDialog::SetPersistence`. The dialog exposes `GetLastApplyResult()` and
`SetOnApplyResult(...)` plus generic failure text through the shared UI. Failed OK/Apply suppresses
the success callback and preserves retryable state; owner destruction/reopen/replacement keeps the
existing weak lifetime/generation guards. A throwing candidate copy returns InvalidData without
effects/history loss; Any::Create holds raw allocation through RAII and publishes type/storage only
after successful construction, avoiding destruction of an incomplete object without a local catch.
The generic JSON loader is unchanged. File storage reports OS errors through error_code/errno;
ordinary allocation exceptions are not disguised as read/write failures.
The current-state guard is also callback code: false/throw or changes made by that guard yield
Conflict after a state recheck. Apply captures CommandService::GetRevision, which changes for edits
inside open transactions as well as visible stack changes. Storage/application/guard callbacks that
edit working values invalidate the old snapshot while preserving the new work. Completion accepts
only Clear's own single revision increment; history/edit work created by Clear observers remains
and causes Conflict instead of being cleared again. In that case the already-published old snapshot's
history has been cleared, while the newly created work/history survives; failures before Clear keep
the original current history.

## Host wiring and current application status

`finish-editor-preferences` closes the settings-only host wiring. SandboxModule now uses the
generic service over `FilePreferenceStorage` and per-user `editor-preferences.json`, with built-ins
and contributors registered before the controlled editor Load and shared persistence injected into
Shell/Dialog. The old three-page archive Save callback is removed from that path. Hub startup also
loads available preferences. T11 implementation and portable/file integration are complete in
`ui-layout-and-chrome-plan.md` §18: at that checkpoint normal/ASan each passed 533 tests, including
42 new closeout cases, and the module settings-source syntax check passed. The later foundation
placement/exception/reentry review has separate evidence in §19. Actual native link/run/display
execution is still a separate gate.

FilePreferenceStorage writes and closes an exclusive temporary file beside the destination before
replacement, preserving previous bytes and cleaning its temporary file on failure. This supplies
the portable storage contract without promising cross-writer compare-and-swap or power-loss
durability. The existing checked codec/unknown-key merge remains unchanged. Built-in registration
no longer clears contributed pages; an owner removes only its own exact registration identities.

Application support is explicit:

| Page / field | Current behavior after successful Apply |
| --- | --- |
| Editor · UI Scale | Immediate Shell/Theme user multiplier, applied once with each main/floating window's own device DPI; startup/hub uses the persisted value |
| General · language | Stored only; no current language consumer |
| General · theme | Stored only; no current theme-selection consumer |
| General · autosave | Stored only; no current autosave consumer |
| Editor · gridSize | Stored only; no current grid-size consumer |
| Editor · snap | Stored only; no current snapping consumer |
| Editor · undoDepth | Stored only; no current history-limit consumer |
| Rendering · vsync | Stored only; renderer application waits for coordinated refactor work |
| Rendering · resolutionScale | Stored only; renderer application waits for coordinated refactor work |

General/Rendering page titles and relevant Editor labels/tooltips expose these stored-only states.
They remain editable, validated and persisted; the UI does not claim unsupported changes take effect
after restart. Adding a real consumer is a separate feature task, especially for rendering values.

Device scale and user preference are independent: `EditorShell::SetUiScale` takes the main device
input, `SetSurfaceScale` a floating device input, and `SetUserUiScale` the user multiplier.
`GetSurfaceUiScale` applies the same normalized product once for every surface, clamped to 0.5–4;
the user range is 0.5–2. The developer environment override remains at the device-input boundary.
Existing theme revision handling invalidates affected layouts after a user-scale change.
The host GUI callback preserves the actual surface id through `PaintGui` into `EditorShell::PaintSurface`.
Shell pointer/key/text entry points select that window's effective theme and prepare layout before
routing, so a main-window modal cannot inherit the last floating paint's scale. Presentation/input
hosts supply the window context; a const geometry query by itself does not infer a surface id.

Generic form ordering now follows explicit `MemberAppearance.order` when `hasOrder` is set, then
reflected `CommonPropertyKey::ORDER`, then zero. Ties preserve existing reflection iteration,
currently the std::map key order, not source registration order. Sort occurs before constructing
root/nested fields to preserve their owned inline storage and child-descriptor addresses.

The user-authorized exception is settings-only SandboxModule registration/load/injection/UI-scale,
necessary service lifetime declarations, and preserving the existing GUI callback's surface id for
that scale policy. Renderer initialization/tick/surface creation/interfaces/resources/refactor code,
RHI/RDG/backends and Core Event remain protected. Windows/native display/DPI/GPU acceptance stays
open; T11-A/B/C evidence remains in the canonical plan's §14/§16/§17.

## Adding a settings page

1. Add a struct in core (or a plugin) and register it (members + `LABEL` / `RANGE_*` / enum names).
2. `PreferencePageRegistry::Get()->Add({ id, title, &instance, GetTypeNode(TypeInfo<T>::RegisteredId()) })`.
3. The dialog renders it automatically — no per-page UI code.
4. Optionally attach page validation/application callbacks and use the portable persistence service
   through the host-controlled file adapter. Register contributors before the host Load boundary;
   runtime registration/reload and provider/DLL lifetime still require their existing ownership rules.
