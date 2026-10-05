## ADDED Requirements

### Requirement: Inspector renders through the reflected-form framework

The inspector panel SHALL render the selected reflected data through the `editor-reflected-form`
framework and SHALL resolve the selection through an `IEditorPropertySource`, not through
panel-specific property code.

#### Scenario: Selection resolved by the source

- **WHEN** the selection changes to an item the source resolves to reflected data
- **THEN** the inspector SHALL bind that data to a reflected-form view and show its members

#### Scenario: Empty selection

- **WHEN** no selection is resolved or no source is provided
- **THEN** the inspector SHALL show an empty state and SHALL NOT crash
