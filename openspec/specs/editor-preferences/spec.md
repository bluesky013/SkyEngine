# editor-preferences Specification

## Purpose
TBD - created by archiving change editor-preferences-dialog. Update Purpose after archive.
## Requirements
### Requirement: Preferences dialog
The editor SHALL provide a modal Preferences dialog opened from the File menu, with a category list on the
left and the selected page on the right, and SHALL close on OK or Cancel.

#### Scenario: Open from the menu
- **WHEN** the user selects `File > Preferences…`
- **THEN** the Preferences dialog SHALL open, focused, over the editor content

#### Scenario: Modal input
- **WHEN** the dialog is open
- **THEN** pointer and keyboard input SHALL be captured by the dialog and the dock content SHALL NOT receive it

#### Scenario: Close
- **WHEN** the user presses OK, Cancel, or Esc
- **THEN** the dialog SHALL close and return input to the editor

### Requirement: Category pages contributed by modules
Preferences SHALL be organized into named categories/pages that modules contribute; the shell SHALL NOT
hard-code subsystem settings.

#### Scenario: Registered pages appear
- **WHEN** a module registers a preference page
- **THEN** that page SHALL appear as a category in the dialog and its settings SHALL render in the page host

#### Scenario: Built-in categories
- **WHEN** the editor starts
- **THEN** at least General, Editor, and Rendering pages SHALL be present, contributed through the
  registration model rather than the shell

### Requirement: Apply, OK, Cancel and Reset
The dialog SHALL edit owned working copies of reflected settings through a session-local command service. OK and Apply SHALL write reflected writable members to still-matching live page registrations and notify the host to persist/apply them. Cancel, Escape, Close and destruction SHALL discard unapplied working values and local history without changing the editor-wide history. Reset SHALL restore the page's reflected type defaults within the same local session. Committed changes SHALL take effect where the owning subsystem supports them.

#### Scenario: Cancel discards
- **WHEN** the user changes settings and presses Cancel, Escape or Close
- **THEN** unapplied working changes and local history SHALL be discarded
- **AND** the live settings and editor-wide document history SHALL remain unchanged

#### Scenario: Apply commits
- **WHEN** the user changes settings and presses Apply
- **THEN** reflected writable members SHALL be written to matching live pages and the host SHALL be notified
- **AND** local history SHALL be cleared before that notification
- **AND** discarding subsequent edits SHALL preserve the applied live values

#### Scenario: OK closes an applied session
- **WHEN** the user presses OK
- **THEN** the current working values SHALL be applied and that session SHALL close
- **AND** a new session opened by the applied callback SHALL NOT be closed by the previous OK operation

#### Scenario: Reset
- **WHEN** the user chooses Reset on a page
- **THEN** that page's fields SHALL return to their type defaults through local undoable edits
- **AND** those values SHALL remain unapplied until Apply or OK

#### Scenario: Undoable cross-page edit
- **WHEN** the user edits fields on multiple pages and switches pages
- **THEN** Undo and Redo SHALL traverse that session's changes across those pages
- **AND** those shortcuts SHALL NOT traverse editor-wide document history

#### Scenario: Reopen
- **WHEN** Preferences opens again after Apply, Cancel or Close
- **THEN** working values SHALL be copied from the current live settings and local history SHALL start empty

### Requirement: Per-user persistence
Committed preferences SHALL persist per user and SHALL be restored on the next startup; unknown keys in a
stored file SHALL be skipped without failing to load.

#### Scenario: Save and restore
- **WHEN** the user commits settings and restarts the editor
- **THEN** the dialog SHALL show the previously committed values

#### Scenario: Forward-compatible load
- **WHEN** the stored file contains keys not known to the current build
- **THEN** loading SHALL succeed and those keys SHALL be ignored

### Requirement: Reflection-driven settings model
Settings SHALL be expressed as reflected data types (plain structs) registered with the reflection
context and decorated with UI metadata (label, order, range, enum names); the model SHALL be
UI-toolkit-free, SHALL provide each setting with a default, and SHALL be testable headlessly. The set
of preference pages SHALL be dynamic (modules register page types), while each page's fields are a
fixed reflected type; the shell SHALL NOT hand-draw per-type controls.

#### Scenario: Defaults
- **WHEN** a setting has no stored value
- **THEN** the model SHALL report its declared (default-constructed) value

#### Scenario: Metadata drives the control
- **WHEN** a page renders a reflected field
- **THEN** the control SHALL be chosen from the field's reflected kind + metadata (bool / integer /
  float slider / enum dropdown), not from a per-type switch

#### Scenario: Registered pages
- **WHEN** a module registers a preference page type
- **THEN** that page SHALL appear as a category and its fields SHALL render through the generic
  reflected form

### Requirement: Owned values and stable page registrations
A Preferences session SHALL own value copies with safe reflected copy semantics, including owning string and sequence values. Editable pages SHALL have unique nonempty ids, copyable reflected types and live registration identities. Registration identity SHALL distinguish re-registration at an unchanged object address. Session commands SHALL target stable owned working objects and SHALL be discarded before those objects are released.

#### Scenario: Deep value cancellation
- **WHEN** a working string or sequence is edited and the session is discarded
- **THEN** the live value SHALL remain valid and unchanged without byte-copy restoration

#### Scenario: Reorder
- **WHEN** existing page registrations are reordered
- **THEN** their working-object addresses, selected page id and local cross-page history SHALL be preserved

#### Scenario: Remove or replace
- **WHEN** a page is removed, replaced with a different type or re-registered with the same id, type and address
- **THEN** reconciliation SHALL clear old history before releasing old working values
- **AND** the replacement SHALL start from its current live values

#### Scenario: Changed registration during Apply
- **WHEN** a setter notification closes/replaces the session or invalidates a page registration
- **THEN** Apply SHALL stop subsequent writes and SHALL NOT emit the success callback

### Requirement: Session callback lifetime
Preferences SHALL detach forms before replacing owned working storage and SHALL destroy child forms before their local services. Notification-capable dialog transitions and event dispatch SHALL retain local services until that dispatch returns. Weak owner/binding checks SHALL reject further access after destruction. Model transitions requested during a committed form edit SHALL wait until that widget event has returned.

#### Scenario: Callback replaces or destroys the dialog
- **WHEN** an applied, closed or local-history notification reopens, replaces or destroys the dialog
- **THEN** the previous operation SHALL NOT access released dialog/form storage or clear the new session's history

#### Scenario: Transition requested during an edit
- **WHEN** a synchronous edit observer requests Close, Open or model replacement
- **THEN** the current field binding SHALL remain valid until the committed widget dispatch returns
- **AND** the requested transition SHALL run at a subsequent safe event/layout boundary

### Requirement: Preferences uses shared container composition
Preferences SHALL compose category navigation, reflected page content, and footer actions through shared UI containers using UiTheme dimensions and explicit logical allocation sizes. The selected page SHALL remain a generic ReflectedFormView bound to the existing session-owned working page and local history. Layout SHALL finish before paint/input. Compatibility rectangle queries, paint, clipping, and hit-testing SHALL consume container-arranged geometry rather than independent per-control placement formulas. Painting SHALL NOT create or repair the layout tree.

#### Scenario: First layout precedes paint
- **WHEN** Preferences opens and is arranged before its first Paint
- **THEN** category rows, page viewport and footer actions SHALL already have their final bounds and logical allocations for paint and input

#### Scenario: Query and input share the arranged tree
- **WHEN** a caller queries a category or action rectangle after layout and sends pointer input within its visible bounds
- **THEN** hit-testing SHALL target the same arranged element that painting uses
- **AND** invalid indices and points outside the relevant clipped region SHALL NOT resolve to an active category/action

#### Scenario: Fractional origin retains logical allocation
- **WHEN** the dialog is arranged at a fractional origin or scaled size
- **THEN** nested measurement and arrangement SHALL use their explicit logical sizes without deriving a contradictory budget from rounded endpoint subtraction

#### Scenario: Paint consumes a prepared composition
- **WHEN** page registration changes after layout and Paint runs before the next explicit preparation boundary
- **THEN** Paint SHALL consume the prepared tree without synchronizing registrations or creating/rearranging category/action elements
- **AND** the next layout, input entry, explicit SynchronizeLayout or compatibility geometry query SHALL prepare the changed composition

### Requirement: Category and reflected page scroll independently
The category list SHALL have a bounded vertical scrolling viewport independent of the selected reflected page's scroll state. Scrolling SHALL route by the visible hovered region, while active reflected popup/capture interactions retain their established routing priority. Category/content scrolling SHALL NOT move footer actions. Offscreen or clipped category rows SHALL NOT be selectable through another region, and scroll offsets SHALL remain clamped after page/extent/viewport changes.

#### Scenario: Select a category beyond the initial viewport
- **WHEN** contributed pages overflow the category viewport and the user scrolls to a later visible row
- **THEN** that row SHALL be selectable and bind its session page through the existing generic form
- **AND** rows outside the viewport SHALL NOT receive selection input

#### Scenario: Wheel stays in the hovered region
- **WHEN** the category list and selected form both overflow and wheel input occurs over one of them
- **THEN** only that region's scroll offset SHALL change and the footer SHALL retain its arranged position

#### Scenario: Form popup keeps input priority
- **WHEN** a reflected page owns an active popup or captured interaction extending beyond the page viewport
- **THEN** category/footer routing SHALL preserve its existing delivery and cancellation semantics

#### Scenario: Foreign pointer cannot release form capture
- **WHEN** a form interaction is captured by one pointer and another pointer sends MOVE, UP or cancellation
- **THEN** the unrelated pointer SHALL NOT modify or end the captured form interaction

#### Scenario: Matching cancellation permits fresh capture
- **WHEN** the owning pointer is cancelled during a form interaction
- **THEN** the dialog SHALL clear its capture and forward cancellation safely to the form before a later pointer starts a new interaction

### Requirement: Category scrollbar stays within its reserved track
The category scrollbar SHALL paint through UiSkin using its actual reserved track rectangle with the viewport height. Its theme-sized thumb SHALL remain inside that rectangle at fractional, ordinary and increased scale and for small valid tracks. Content that fits the viewport SHALL draw no thumb. This painter SHALL NOT create a second layout or scroll owner.

#### Scenario: Reserved track changes with scale
- **WHEN** a reserved category scrollbar track is painted at 0.5x, 1x or 2x theme scale
- **THEN** emitted thumb geometry SHALL remain within the supplied track and represent the clamped scroll offset

#### Scenario: Track is tiny or content fits
- **WHEN** the reserved track is smaller than the normal themed thumb or the content does not overflow
- **THEN** thumb geometry SHALL be bounded to the valid track or omitted when content fits, without negative/inverted draw bounds

### Requirement: Preferences responds to available bounds and theme
Preferences SHALL derive panel, category, row, footer, action and spacing dimensions from UiTheme and constrain composition to the available surface. Smaller windows SHALL use bounded category/page allocations and an adapted shared-container footer configuration rather than overlapping fixed targets. Long labels SHALL stay within their allocated painted/hit regions. Theme revisions, same-scale metric changes, resize, page reconciliation and reopen SHALL prepare the affected tree before subsequent paint/input. Degenerate bounds MAY yield clipped or empty regions but SHALL NOT produce negative/NaN allocations or active targets outside their visible region.

#### Scenario: Smaller window adapts the layout
- **WHEN** available width cannot fit the normal footer row and nominal category width
- **THEN** container allocation SHALL adapt to the constrained bounds with finite nonnegative regions and nonoverlapping visible footer action targets

#### Scenario: Long category label stays contained
- **WHEN** a contributed category title exceeds its allocated row width
- **THEN** its text SHALL be clipped or ellipsized within that row and SHALL NOT extend the category's hit target or overlap the reflected page

#### Scenario: Scale changes before the next paint
- **WHEN** theme scale or same-scale layout metrics change after the dialog has been constructed
- **THEN** copied container inputs and affected layout state SHALL update before paint using the new theme without requiring dialog reconstruction

### Requirement: Composition preserves Preferences session behavior
Container and row rebuilding SHALL preserve the existing PreferencesSession working objects, local history, page registration reconciliation, Apply/OK/Cancel/Reset behavior, and guarded transition/callback lifetime. The form SHALL remain bound to session-local services and SHALL be unbound/destroyed before those services or working objects are released. Recomposition alone SHALL NOT clear valid session history or apply settings. Generic persistence/application and native module wiring SHALL remain unchanged by this layout migration.

#### Scenario: Cross-page undo survives navigation composition
- **WHEN** the user edits multiple working pages, scrolls/selects categories, or the dialog resizes
- **THEN** Undo/Redo SHALL retain the session's existing cross-page behavior and SHALL NOT consume editor-wide document history

#### Scenario: Apply and Cancel retain their baselines
- **WHEN** the user applies working changes, edits again and then cancels after a layout/page change
- **THEN** the applied live values SHALL remain and the subsequent unapplied working changes SHALL be discarded according to the existing session contract

#### Scenario: Layout transition notifies an external callback
- **WHEN** a safe pending transition or session operation triggers an observer that closes, reopens, replaces or destroys Preferences
- **THEN** the composition code SHALL preserve existing service retention and lifetime/generation checks without accessing released form/row/dialog storage or cancelling a replacement session

### Requirement: Session Apply orders persistence before guarded publication
PreferencesSession SHALL provide a detailed Apply result while retaining the bool Apply compatibility entry. Detailed Apply SHALL verify/capture owned candidates, validate all candidates, optionally persist through the injected service, perform guarded live writes and page application, and only then reach one guarded history-clear boundary. Complete success SHALL require that boundary's callbacks/current guard add no conflicting work. Read/serialization/validation/failed-Write results SHALL leave live settings unchanged and retain working values/history. Post-save failures before Clear SHALL retain the current working values/history where the owner remains current and report accumulated effects without rollback. A Clear-observer conflict SHALL preserve the observer's new work/history even though the published old snapshot's history has already been cleared.

#### Scenario: Validation or storage fails
- **WHEN** validation, serialization, read or Write fails before publication
- **THEN** working values/history SHALL remain available for correction/retry, live settings SHALL remain unchanged, and no success notification SHALL be emitted

#### Scenario: Save succeeds but publication fails
- **WHEN** durable Save succeeds and guarded live/application publication later fails
- **THEN** the result SHALL report partial effects and local history SHALL NOT be cleared merely because storage succeeded

#### Scenario: Apply completes successfully
- **WHEN** every required validation, storage and publication/application stage succeeds
- **THEN** the guarded history-clear boundary SHALL complete before success notification and subsequent unapplied working edits MAY be discarded without undoing that completed Apply

#### Scenario: No persistence service is configured
- **WHEN** an existing caller uses bool Apply or detailed Apply without an injected persistence service
- **THEN** validation and guarded publication SHALL remain available without storage effects and bool Apply SHALL report complete success only

### Requirement: Cancel does not roll back previously reported partial effects
Cancel/Close SHALL discard the current session's working values and history. Following a failed Apply with partial effects, they SHALL NOT promise to restore already-written storage, live values or provider application effects. Reopen SHALL capture current live values, and retry MAY repeat prior publication/application work through retry-safe providers.

#### Scenario: Cancel follows a post-save failure
- **WHEN** Apply reports storage/live/application effects and the user cancels the remaining working session
- **THEN** working values/history SHALL be discarded while those already-performed effects remain outside Cancel's rollback contract

### Requirement: Dialog exposes optional portable persistence and failure results
PreferencesDialog SHALL support an optional shared portable persistence service, expose its structured operation result, and present generic failure information through existing shared UI. Failed Apply/OK SHALL leave the current dialog/session open where its owner remains current, preserve retryable working/history state, suppress the legacy success callback and avoid an OK close. The existing void onApplied callback SHALL remain a success-only compatibility notification rather than the checked storage protocol. Generic fields, container composition and local/global history isolation SHALL remain unchanged.

#### Scenario: Failed OK remains retryable
- **WHEN** OK encounters a validation/storage/publication/application failure while the dialog remains current
- **THEN** it SHALL remain open, expose the failure/effect result and allow correction or retry without emitting a successful applied/closed notification

#### Scenario: Retry then success
- **WHEN** the failure is corrected and a later Apply succeeds
- **THEN** the detailed result SHALL describe the successful operation, local history SHALL clear through the existing guarded boundary, and the compatibility success notification SHALL run

### Requirement: Old operations do not consume replacement session history
Notification-capable validation/storage/publication/history operations SHALL retain required service/value lifetime and recheck session/owner generation after callbacks. A destroyed/reopened/replaced dialog or session SHALL stop the previous operation from accessing released storage or clearing new history. Busy/reentrant operations SHALL report their result rather than unsafely nesting the same publication operation.

#### Scenario: Current guard invalidates itself
- **WHEN** an isCurrent callback returns false, throws, or closes/reopens/destroys/edits its owner during the check
- **THEN** the old operation SHALL return Conflict and SHALL NOT treat a true return from a state-mutating guard as permission to access stale borrowed state

#### Scenario: Callback replaces the session
- **WHEN** a page/storage/history callback opens a replacement session during Apply
- **THEN** the previous operation SHALL stop under its generation/owner guards and SHALL NOT clear replacement working values or history

#### Scenario: Callback destroys the dialog
- **WHEN** a callback destroys the dialog while its operation is running
- **THEN** retained services MAY finish their guarded call but no later access SHALL use released dialog/form storage

### Requirement: Callback edits invalidate an Apply snapshot by history revision
CommandService SHALL expose a mutation revision that changes for edits inside an open transaction as well as ordinary history operations. Apply SHALL compare that revision with its captured revision around callback-capable stages, including its current guard. New edits from storage/application/guard callbacks SHALL invalidate the old snapshot, return Conflict and preserve the newer working/history state. After the old history's successful-publication Clear, only its own single revision increment SHALL be accepted; edits/history created by Clear observers or a final guard SHALL remain and SHALL NOT be cleared again by the old operation.

#### Scenario: Callback edits inside a transaction
- **WHEN** storage, application or a current guard edits the working page inside an open transaction without changing stack counts
- **THEN** the changed revision SHALL stop the stale Apply snapshot before further publication/history clearing and preserve that pending work

#### Scenario: Clear observer creates new history
- **WHEN** a history Clear observer or its subsequent guard creates another edit/history entry
- **THEN** the old operation SHALL return Conflict after observing more than its expected one revision increment and SHALL leave the new work intact

### Requirement: Host preferences use generic persistence and preserve contributors
The host SHALL register its owned built-in pages and contributed pages before its controlled generic preference Load, then inject the shared file persistence service into Shell/Dialog Apply rather than retain a three-built-in-page Save-only gate. The hub SHALL load available preferences before applying its settings. Built-in registration lifecycle SHALL remove only its exact owned registration ids and reject unrelated reserved-id collisions without clearing contributed pages. Existing saved page envelopes and unknown/temporarily absent plugin values SHALL retain the prior checked codec contract.

#### Scenario: Plugin pages survive host setup
- **WHEN** a contributor registers a nonreserved page and the host binds/rebinds its built-ins
- **THEN** that contributor SHALL remain registered and participate in generic Load/Save at the controlled registration boundary

#### Scenario: Built-in owner is released
- **WHEN** a built-in binding resets or is destroyed after a registration was replaced
- **THEN** it SHALL remove only its own still-matching registration ids and SHALL NOT remove a newer or plugin-owned page

#### Scenario: Existing saved file starts a new host
- **WHEN** the host loads an existing pageId/classId/elements document after registration
- **THEN** registered built-ins/contributors SHALL use the generic checked Load and later Apply SHALL be gated by the injected persistence service

### Requirement: Unsupported built-in setting effects are presented as stored only
Only editor.uiScale SHALL claim a connected immediate Shell/Theme application in this batch. The other eight current built-in values—language, theme, autosave, gridSize, snap, undoDepth, vsync and resolutionScale—SHALL remain editable/validated/stored but SHALL be identified as stored-only where no consumer exists. The UI/docs SHALL NOT claim they apply immediately or on restart. Rendering consumers SHALL remain outside the current RHI/RDG/backend exclusion.

#### Scenario: User edits a stored-only value
- **WHEN** the user changes one of those eight built-in values and Apply succeeds
- **THEN** its value SHALL be stored through the generic protocol and its presentation SHALL distinguish storage from an unimplemented feature effect

#### Scenario: User changes UI scale
- **WHEN** editor.uiScale applies successfully
- **THEN** its callback SHALL update the shared Shell/Theme user multiplier while preserving the previous result-aware history/partial-effect contract

### Requirement: Reflected copy reentry preserves source and replacement state
Preference capture SHALL own page descriptors before invoking reflected copying and SHALL recheck its current guard after each copy. Session Apply SHALL retain working source objects across End/Begin reentry. Synchronize SHALL build candidates without releasing existing working objects and SHALL publish them only while its generation, registry identities and history revision remain current.

#### Scenario: Copy mutates the registry
- **WHEN** a reflected copy clears or replaces the registry
- **THEN** capture SHALL stop before another borrowed page access and SHALL leave its output unchanged on Conflict

#### Scenario: Copy ends the source session
- **WHEN** a reflected constructor ends or replaces the session and continues reading its source
- **THEN** that source SHALL remain alive until copying completes and the old operation SHALL not publish into the replacement session

#### Scenario: Synchronization copy creates newer history
- **WHEN** a reflected constructor edits an existing working object or opens another session
- **THEN** the stale synchronization SHALL retain that newer state/history and SHALL NOT release its command targets

### Requirement: Opening Preferences stops after owner destruction
OpenPreferences SHALL verify shell lifetime, dialog identity and chrome generation after callback-capable setup/open operations. History observers SHALL be allowed to destroy or replace the owner without a later stale continuation.

#### Scenario: Reopening destroys the shell
- **WHEN** a SetModel or Open history notification destroys the shell during Preferences reopening
- **THEN** OpenPreferences SHALL return without accessing released shell or dialog storage
