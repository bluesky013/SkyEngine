## ADDED Requirements

### Requirement: Shared action chrome is independent of the shell
ActionBarBuilder, ToolBar, and MenuBar SHALL be provided by EditorWidgets and SHALL be usable without linking EditorShell. They SHALL continue to consume EditorActionRegistry, scope, icon, and accelerator data and SHALL invoke actions through a host-supplied callback rather than a concrete Shell instance. Host-only menu entries and policy SHALL remain with their host.

#### Scenario: Feature consumer builds scoped chrome
- **WHEN** a consumer linked only to the shared editor framework builds toolbar/menu items for its active scope
- **THEN** the builder SHALL use that scope's registered actions without requiring Shell

#### Scenario: Action invocation reaches the supplied host
- **WHEN** a shared toolbar/menu item is activated
- **THEN** it SHALL pass the registered action id to the supplied invoker so the host can apply its normal policy

#### Scenario: Accelerator resolution stays shared
- **WHEN** a registered accelerator is resolved outside the Shell
- **THEN** the shared builder SHALL return the same scoped action id used to build its toolbar/menu entries
