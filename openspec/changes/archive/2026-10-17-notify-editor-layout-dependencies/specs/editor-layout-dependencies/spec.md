## ADDED Requirements

### Requirement: Controlled editor hosts synchronize dependencies before allocation
Known migrated editor hosts SHALL explicitly synchronize pending layout dependencies before querying child preferred size or requesting incremental arrangement. Synchronization SHALL occur before paint and SHALL make newly observed data available to the same allocation pass. Controlled host-owned forms and composition nodes MAY opt out of volatility only after their layout inputs have notification paths. Unknown custom/derived hosts SHALL retain conservative layout behavior unless they explicitly satisfy the incremental notification contract.

#### Scenario: External change precedes a clean frame
- **WHEN** an injected dependency changes after a controlled editor host completes layout and the host's allocated rectangle remains unchanged
- **THEN** the next host allocation SHALL synchronize that dependency and consume its notified layout work before paint

#### Scenario: Unknown provider retains polling compatibility
- **WHEN** a custom provider element changes externally without a setter notification and retains default volatility
- **THEN** its containing incremental host SHALL execute the branch and observe the new layout rather than declare the provider stable

#### Scenario: Stable host repeats unchanged input
- **WHEN** a controlled editor host completes its work and no dependency, theme, rectangle, or volatile descendant changes
- **THEN** another incremental allocation SHALL be eligible to skip its completed arrangement work

### Requirement: World config supports explicit document and refresh notifications
WorldConfigPanel SHALL support explicit document binding and an explicit refresh notification for config data/storage changes at the same document address. Binding/refresh SHALL rebind the reflected config as necessary and invalidate layout before reuse. EditorShell SHALL propagate its document changes to applicable main and floating world-config views. The legacy DocumentProvider path SHALL remain compatible and volatile so unnotified provider changes are still observed during layout.

#### Scenario: Document changes through the shell
- **WHEN** the shell opens, closes, or replaces its world document and the panel rectangle is unchanged
- **THEN** applicable world-config views SHALL bind the new document or empty state before the next paint

#### Scenario: Same document replaces config storage
- **WHEN** a document owner rebuilds or relocates config storage and explicitly requests a world-config refresh
- **THEN** the panel SHALL rebind to the current config storage instead of retaining a stale reflected data pointer

#### Scenario: Shell repeats the same document pointer
- **WHEN** a document owner replaces config storage and calls EditorShell::SetWorldDocument with its current document pointer
- **THEN** applicable attached, hidden, and floating world-config views SHALL receive a refresh and rebind to the current storage

#### Scenario: Legacy provider returns another document
- **WHEN** a legacy provider returns a different document without a setter notification between incremental frames
- **THEN** the panel SHALL continue to detect the change and update its bound config through the conservative path
