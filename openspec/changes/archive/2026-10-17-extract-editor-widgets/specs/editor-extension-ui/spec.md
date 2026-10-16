## MODIFIED Requirements

### Requirement: Editor extensions do not link the shell
Feature editor-extension modules that supply custom asset views SHALL depend on EditorUi and EditorCore as needed, and SHALL be able to link EditorWidgets for reusable editor presentation without linking EditorShell. EditorUi SHALL retain the UI-facing provider seam; reusable skin/form/chrome implementations SHALL be consumed from EditorWidgets rather than Shell.

#### Scenario: Aurora editor extension
- **WHEN** `engine/aurora/editor` (`Aurora.Render.Editor`) is configured
- **THEN** it SHALL link EditorWidgets and the required EditorCore/EditorUi/UI dependencies and SHALL NOT link EditorShell

#### Scenario: Custom view still works
- **WHEN** the aurora material editor provides a custom `UIElement` content widget
- **THEN** it SHALL register through EditorUi's AssetViewProviderRegistry without any EditorShell dependency

#### Scenario: Low-level extension remains small
- **WHEN** an extension only needs the asset-view provider seam and low-level primitives
- **THEN** it SHALL be able to use EditorUi without acquiring an EditorWidgets or EditorCore dependency through EditorUi
