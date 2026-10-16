## ADDED Requirements

### Requirement: Binding replacement distinguishes cancellation from abandonment
ReflectedFormView SHALL distinguish live-model cancellation from invalid-binding abandonment. CancelInteractions and Refresh SHALL require the current model to remain valid and SHALL restore uncommitted live previews before clearing interaction state. Bind, Unbind, and destruction SHALL abandon obsolete callbacks, popups, capture, drag, hover, and inline-edit targets without accessing the former model. Unbind SHALL leave no valid model for a later pending notification to rebuild. Form-local widget registrations and context dependencies SHALL remain unchanged by rebinding.

#### Scenario: Bind after the old model has been released
- **WHEN** a form has an active color preview and binds another object after its old model storage was released
- **THEN** it SHALL clear the obsolete interaction without reading or writing the released model and subsequent input SHALL NOT commit against that binding

#### Scenario: Host cancels before valid model replacement
- **WHEN** a host calls CancelInteractions while the old model remains valid, then replaces its storage and binds the replacement
- **THEN** the uncommitted preview SHALL be restored before release and the new binding SHALL have no old interaction state

#### Scenario: Escape restores a live preview
- **WHEN** Escape cancels an active preview whose model remains valid
- **THEN** the model SHALL return to its opening value without adding a command

#### Scenario: Unbind survives later notifications
- **WHEN** a form unbinds after model release and receives property or command notifications
- **THEN** later synchronization SHALL NOT rebuild the former binding or access its data

#### Scenario: World config storage is replaced at the same document address
- **WHEN** WorldConfigPanel refreshes after its document's config storage was replaced during an active preview
- **THEN** it SHALL abandon the old binding before resolving the new config and display the new config without an old rollback

### Requirement: Obsolete binding callbacks become inert
A form SHALL expose a weak binding-generation lifetime distinct from its service subscription lifetime. Abandonment SHALL expire that generation before clearing widgets. Cancellation SHALL permit live rollback before expiring it. Built-in color and inline-edit callbacks SHALL reject an expired generation before accessing a field or host. Custom widgets SHALL drop obsolete target pointers for either reset mode and SHALL NOT access model data during Abandon.

#### Scenario: Callback belongs to an old field generation
- **WHEN** a retained callback is invoked after its binding was reset, rebound, unbound, or destroyed
- **THEN** built-in binding callbacks SHALL perform no model or host access

#### Scenario: Form destruction outlives the model
- **WHEN** a form is destroyed after its borrowed model was already released
- **THEN** teardown SHALL abandon interactions without rollback and remove notifier/command subscriptions without model access
