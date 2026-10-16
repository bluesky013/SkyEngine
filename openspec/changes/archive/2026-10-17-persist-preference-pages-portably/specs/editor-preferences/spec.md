## ADDED Requirements

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
