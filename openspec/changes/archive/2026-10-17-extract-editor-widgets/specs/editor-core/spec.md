## ADDED Requirements

### Requirement: Default editor services are shared across DLLs
The default CommandService, PropertyEditorRegistry, and PropertyChangeNotifier returned by EditorCore SHALL have process-wide Environment-backed singleton storage even when static EditorCore code is linked into multiple DLLs. Default consumers SHALL actually use these shared instances. The service classes SHALL remain independently constructible for explicit local/headless contexts, and EditorCore SHALL remain UI-free.

#### Scenario: Modules resolve the same services
- **WHEN** the host and a separately linked dynamic module call the default service getters
- **THEN** they SHALL receive the same command service, metadata registry, and notifier objects

#### Scenario: Metadata and notifications cross the module boundary
- **WHEN** one module registers property metadata or advances the default property notifier
- **THEN** a default form/consumer in another module SHALL observe that metadata or notification

#### Scenario: Commands cross the module boundary
- **WHEN** a default form in a feature module performs an undoable property edit
- **THEN** the host's default CommandService SHALL be able to undo that edit

#### Scenario: Local services remain possible
- **WHEN** a headless test or explicit context constructs independent services
- **THEN** it SHALL be able to use them without altering the process-default services or requiring UI initialization

### Requirement: Asset catalog injection is shared across DLLs
The override set through SetEditorAssetCatalog SHALL have Environment-backed process-shared storage. A host's injected catalog SHALL be returned to widgets and feature consumers in other DLLs; clearing the override SHALL restore the existing default EditorAssetCatalog singleton lookup.

#### Scenario: Feature widget sees host catalog
- **WHEN** the host installs a catalog override and a feature module resolves the editor catalog
- **THEN** the feature module SHALL receive the installed override

#### Scenario: Clearing override restores the default
- **WHEN** the host clears the catalog override
- **THEN** all modules SHALL resolve the shared default catalog rather than retaining a per-DLL override
