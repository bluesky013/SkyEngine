## ADDED Requirements

### Requirement: Explicit document close decisions
Closing or replacing an asset document with unsaved changes or an active edit SHALL display a modal Save and Close / Discard Changes / Cancel Close prompt in its owning window. The prompt SHALL use the document's scoped actions and SHALL block underlying pointer, text and keyboard input. Escape SHALL cancel closing. Document and preview destruction SHALL remain deferred until input dispatch completes.

#### Scenario: Close a modified Graph
- **WHEN** the user closes a modified Graph through its native window or Close action
- **THEN** a visible prompt SHALL offer save, discard and cancel without requiring menu discovery
- **AND** cancel SHALL preserve the document, history and current edit

#### Scenario: Saving fails during close
- **WHEN** Save and Close fails to persist the source
- **THEN** the document and prompt SHALL remain open with a visible failure message and allow retry, discard or cancel
- **AND** repeated save and discard SHALL be disabled while a save is pending

#### Scenario: Close during an active edit
- **WHEN** a field or drag transaction remains active at close
- **THEN** Save and Close SHALL be disabled with an explanation to cancel closing and finish the edit
- **AND** Discard Changes SHALL cancel the interaction and close on a later tick without waiting for the edit to complete

#### Scenario: Independent close decisions
- **WHEN** multiple asset windows are open and one displays a close prompt
- **THEN** its decision SHALL affect only its own document and its prompt SHALL not pass undo, save or editing input to the document below it

## MODIFIED Requirements

### Requirement: Asset preview sessions
EditorCore SHALL extend its consumer-owned asset preview seam from a boolean query to explicit session creation, update/resize and close/status operations, with owned session identity and generation. Registry state SHALL be cross-DLL shared and provider registrations SHALL be revoked only after sessions drain. The shell SHALL host preview outputs through the existing shared renderer; animation semantics SHALL remain in Animation.Editor. Unsupported asset types or absent providers SHALL retain the placeholder. Sessions SHALL NOT own a second Device or producer.

#### Scenario: No provider draws a placeholder
- **WHEN** no provider supports an asset type
- **THEN** the preview region SHALL show its placeholder without claiming a live preview

#### Scenario: Multiple independent sessions
- **WHEN** the same asset is opened in two preview windows
- **THEN** each SHALL have a distinct session/time/output and closing one SHALL NOT destroy the other

#### Scenario: Provider unload with active output
- **WHEN** an extension is unloaded while a preview output is in flight
- **THEN** it SHALL stop session updates, drain CPU work and output retirement, and only then release provider code

### Requirement: Hidden assets do not open the asset viewer
The asset-open handler SHALL NOT open a viewer for a type whose resolved AssetViewFlags is HIDDEN. AuroraSkin SHALL have a registered read-only reflected DATA summary in the Aurora editor extension, showing its owner source, slot/mapping mode and format/revision. Generated Skin editing SHALL occur by owner reimport, not by writing cooked matrices directly.

#### Scenario: Unregistered internal type is not opened
- **WHEN** a type has no settings or data provider and resolves to HIDDEN
- **THEN** the viewer SHALL not open

#### Scenario: Skin opens a reflected summary
- **WHEN** the user opens a generated Skin with its Aurora DATA provider registered
- **THEN** the viewer SHALL show its read-only summary and owner-source identity
