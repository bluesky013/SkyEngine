## ADDED Requirements

### Requirement: Global config panel uses the framework and a config source

The global config panel SHALL obtain named configurations through an `IEditorConfigSource` and render
each through the `editor-reflected-form` framework, and edits SHALL be undoable and written back to the
configuration storage.

#### Scenario: Named configs rendered

- **WHEN** the config source returns named reflected configurations
- **THEN** the panel SHALL render one section per configuration through the reflected-form framework

#### Scenario: Edit is undoable and persisted

- **WHEN** the user edits a configuration value
- **THEN** the change SHALL be applied through the command service and SHALL be observable when the
  configuration is read again
