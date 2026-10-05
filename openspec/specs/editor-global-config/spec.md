# editor-global-config Specification

## Purpose
TBD - created by archiving change editor-property-ui. Update Purpose after archive.
## Requirements
### Requirement: Named configuration source

The editor SHALL resolve global configuration objects through a config source that returns a name and a
reflected data object for each registered configuration, without `EditorCore` depending on the concrete config
type or its storage.

#### Scenario: Registered configs are exposed

- **WHEN** the host has registered one or more named reflected configurations
- **THEN** the config source SHALL return each configuration's name and a reflected data object

#### Scenario: No configs

- **WHEN** no configuration is registered
- **THEN** the config source SHALL return an empty list and the panel SHALL show an empty state

### Requirement: Configuration form panel

The global config panel SHALL render each named configuration as a form using the `editor-reflected-form`
framework, stacked as one collapsible section per named configuration in a single scrollable view.

#### Scenario: Each config becomes a form

- **WHEN** the panel shows multiple named configurations
- **THEN** each configuration SHALL be rendered as its own titled form section

#### Scenario: Reuses the form framework

- **WHEN** a configuration object is rendered
- **THEN** its members, nesting, and controls SHALL be produced by the reflected-form framework, not by
  config-specific layout code

### Requirement: Configuration edits are undoable and persisted

Edits made through the config panel SHALL be routed through the `CommandService` so they are undoable, and SHALL
be written back to the underlying configuration storage so the change is observable through the config source.

#### Scenario: Edit is undoable

- **WHEN** the user edits a configuration value
- **THEN** the underlying configuration SHALL change and a subsequent undo SHALL restore the previous value

#### Scenario: Edit is written back

- **WHEN** the user edits a configuration value
- **THEN** reading that configuration through the config source SHALL return the new value

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

