## ADDED Requirements

### Requirement: Editor extensions own domain edit preparation
The existing EditorExtensionHost SHALL dispatch domain property and detached-component preparation through renderer-independent EditorCore contracts. Results SHALL distinguish unhandled edits, prepared owned commands and explicit errors. Multiple handlers or an invalid prepared result SHALL be rejected; rejection SHALL NOT fall back to an ordinary property write.

#### Scenario: Generic edit without domain handler
- **WHEN** no registered extension handles a property edit
- **THEN** the existing generic reflected editing and undo behavior is used

#### Scenario: Conflicting or failed domain preparation
- **WHEN** multiple extensions handle an edit or an extension reports an error
- **THEN** the operation fails without changing the live World or adding a history entry

### Requirement: Aurora editing policy participates in host transactions
Aurora.Editor SHALL own Aurora-specific actor creation, skeletal mesh binding and animation skeleton/pose editing policy. WorldEditorState SHALL keep generic World/document transactions without depending on concrete Aurora component types. Dependency activation, component attachment and domain changes SHALL be applied and undone as one operation, preserving state when preparation or execution fails.

#### Scenario: Skeletal binding changes and history traversal
- **WHEN** the user changes a skeletal mesh or adds a supported animation component
- **THEN** the extension prepares all related mesh/skin/skeleton/pose changes
- **AND** undo/redo restores or reapplies the complete operation

#### Scenario: Failed attach or document replacement
- **WHEN** attachment or domain execution fails, or a document binding is replaced
- **THEN** failed operations compensate completed steps and do not commit a partial history entry
- **AND** replacing the document clears old commands before releasing their bindings

#### Scenario: Aurora actor creation remains discoverable
- **WHEN** the Aurora editor extension is registered
- **THEN** Cube, Plane, lights, camera and environment creators use the existing actor/action registries
- **AND** unregistering the extension removes its registrations while Empty creation remains available
