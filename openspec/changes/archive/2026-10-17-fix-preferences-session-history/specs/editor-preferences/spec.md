## MODIFIED Requirements

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

## ADDED Requirements

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
