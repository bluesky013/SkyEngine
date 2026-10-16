## ADDED Requirements

### Requirement: Forms support domain-owned edit commands
ReflectedForm SHALL accept an optional UI-free edit-command factory receiving a transient top-level property descriptor and its staged new value. Scalar, nested, reset, sequence-add, and sequence-remove edits SHALL use that seam and emit the committed-change callback once. Without a factory the form SHALL retain generic descriptor-command behavior. A domain factory SHALL NOT retain the descriptor, field, form, or borrowed model as a historical target.

#### Scenario: Disposable model uses a domain command
- **WHEN** an asset form commits an edit using a registered domain factory and later replaces its materialized model
- **THEN** the recorded command SHALL remain independent of the released model's storage

#### Scenario: Sequence and reset respect the same factory
- **WHEN** a domain-bound form adds/removes a sequence element or resets a property
- **THEN** it SHALL create the edit through the same factory and notify its host once

### Requirement: Synchronous domain observers cannot invoke obsolete form callbacks
ReflectedForm SHALL snapshot its factory and changed callback before command dispatch and SHALL use a weak binding generation to suppress the changed callback after a synchronous observer destroys or rebinds the form. Dispatch SHALL NOT retain that generation or access form members after execution. An active-edit counter SHALL remain safe to release independently of the form's lifetime.

#### Scenario: Persistence observer destroys the editing form
- **WHEN** catalog persistence synchronously invokes an observer that destroys the ReflectedForm executing the domain edit
- **THEN** Edit SHALL return without accessing that form and SHALL NOT invoke its obsolete changed callback

#### Scenario: Persistence observer rebinds the same form
- **WHEN** a synchronous observer rebuilds or clears the editing form's binding
- **THEN** the former binding's changed callback SHALL NOT execute after command dispatch
