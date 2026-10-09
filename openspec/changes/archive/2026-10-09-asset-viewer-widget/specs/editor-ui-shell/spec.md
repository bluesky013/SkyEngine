## ADDED Requirements

### Requirement: Shell hosts the asset viewer

`EditorShell` SHALL host the asset viewer widget as an overlay dialog (like the file browser), expose
`OpenAssetViewer(uuid)`, include it in modal input routing and layout, and SHALL route asset double-click /
`asset.open` to it when no type-specific `IEditorAssetEditor` is registered. The type-specific editor SHALL
take precedence when registered.

#### Scenario: Double-click opens the viewer

- **WHEN** an asset is double-clicked and no type-specific editor is registered
- **THEN** the shell opens the asset viewer for that uuid and it becomes the active modal

#### Scenario: Type-specific editor wins

- **WHEN** an `IEditorAssetEditor` is registered for the asset type
- **THEN** opening routes to that editor instead of the generic viewer

#### Scenario: Viewer participates in input routing

- **WHEN** the viewer is open
- **THEN** pointer/key/text input is routed to it (Escape closes) and normal panels do not receive it
