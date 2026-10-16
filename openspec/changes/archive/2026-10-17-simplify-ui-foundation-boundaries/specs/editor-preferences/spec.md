## ADDED Requirements

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
