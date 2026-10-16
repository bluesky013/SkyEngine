## ADDED Requirements

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
