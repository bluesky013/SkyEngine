# ui-data Specification

## Purpose
TBD - created by archiving change engine-ui-system. Update Purpose after archive.
## Requirements
### Requirement: UI document format
The UI system SHALL define a declarative UI document format describing a tree of elements. Each node SHALL
carry a `type`, an optional `name`, `props` (grouped layout and visual/content values), optional `bindings`, and
`children`. Documents SHALL reference style/theme classes rather than raw visual constants.

#### Scenario: Document describes a tree
- **WHEN** a document declaring a panel with a text and an image child is loaded
- **THEN** the resulting element tree SHALL contain the panel with the text and image as children in declaration order

#### Scenario: Unknown type diagnosed
- **WHEN** a document node declares a `type` not present in the element-type registry
- **THEN** the loader SHALL report a diagnostic identifying the unknown type and SHALL NOT crash

### Requirement: Element-type registry
The UI system SHALL provide an extensible element-type registry mapping a document `type` string to an element
factory, so new widget types can be registered without modifying the loader.

#### Scenario: Register a custom type
- **WHEN** a new element type is registered with the registry
- **THEN** documents using that type SHALL instantiate it through the registered factory

### Requirement: Runtime document loader
The UI system SHALL provide a runtime loader that parses a UI document and builds a `UIElement` tree, exposing
loaded elements by name for lookup.

#### Scenario: Load and look up by name
- **WHEN** a document with a named element is loaded
- **THEN** the loader SHALL return a tree from which that element can be retrieved by its name

#### Scenario: Missing referenced asset
- **WHEN** a document references an asset that cannot be resolved
- **THEN** the loader SHALL report a diagnostic and SHALL continue loading the remainder of the document

### Requirement: Style and theme resources
Styles and themes SHALL be loadable as data. Documents SHALL resolve appearance through style classes so that
replacing the active theme re-skins loaded documents without editing them.

#### Scenario: Theme swap re-skins document
- **WHEN** the active theme is replaced after a document is loaded
- **THEN** elements referencing style classes SHALL use the new theme's style values

### Requirement: Data-context property binding
The UI system SHALL provide a data context exposing named values with change notification, and property
bindings that map a source path on the data context to a target element property. Bindings SHALL re-evaluate
when the context changes or when the frame updates.

#### Scenario: Bound property updates
- **WHEN** a value on the data context changes
- **THEN** elements bound to that path SHALL reflect the new value without reloading the document

#### Scenario: Unbound value
- **WHEN** a binding references a path that does not exist on the data context
- **THEN** the binding SHALL report a diagnostic and the target property SHALL retain its last value

### Requirement: Binding provider seam
Path resolution SHALL go through an `IUIDataProvider` abstraction so the UI data layer does not hard-depend on
the reflection system.

#### Scenario: Custom data provider
- **WHEN** a custom data provider is installed
- **THEN** binding path resolution SHALL use that provider

### Requirement: Asset resolution seam
Document asset references (textures, fonts, sub-documents) SHALL be resolved through an `IUIAssetResolver`
abstraction so the UI data layer remains render-agnostic.

#### Scenario: Data layer has no render dependency
- **WHEN** the UI data layer is built without render or Aurora include paths
- **THEN** compilation SHALL succeed

### Requirement: C++ declarative builder parity
The UI system SHALL provide a C++ declarative builder that constructs the same element tree and attaches the
same bindings as the document loader, so a screen built in code behaves identically to one loaded from a
document.

#### Scenario: Builder matches loader
- **WHEN** the same screen is built with the C++ builder and loaded from an equivalent document
- **THEN** both SHALL produce the same element hierarchy and bound properties

#### Scenario: Tests avoid asset IO
- **WHEN** a UI test builds a screen with the C++ builder
- **THEN** it SHALL not require loading a UI document from disk

### Requirement: Document scope
This milestone SHALL provide the document model, loader, element-type registry, style/theme resources, and
property binding. A visual authoring tool, document-to-code generation, event graphs, and hot reload SHALL be
out of scope.

#### Scenario: No authoring or codegen dependency
- **WHEN** the UI build dependencies are inspected
- **THEN** no UI authoring tool or document code generator SHALL be required to build or run the system

### Requirement: Built-in binding converters
The binding system SHALL provide built-in converters `remap`, `format`, and `boolToVisible` that transform a
source value into a target value, with converter arguments supplied by the document.

#### Scenario: remap scales a value
- **WHEN** a binding uses `remap` with an input range and an output range
- **THEN** the target value SHALL be the source value linearly mapped from the input range into the output range

#### Scenario: boolToVisible passes a bool
- **WHEN** a binding uses `boolToVisible`
- **THEN** the target SHALL receive the source boolean value unchanged

### Requirement: Binding evaluation timing
Bindings SHALL re-evaluate when the data context changes, and SHALL NOT re-apply when the context is unchanged
since the last evaluation.

#### Scenario: Apply on change
- **WHEN** a value on the data context changes and bindings are applied
- **THEN** bound targets SHALL reflect the new value

#### Scenario: Skip when unchanged
- **WHEN** bindings are applied again without any data-context change
- **THEN** the system SHALL report that no update was needed

### Requirement: Load diagnostics
The loader SHALL return diagnostics describing content problems (unknown element type, unresolved asset,
unknown property path, duplicate name) with severity, node path, code, and message, and SHALL NOT throw.

#### Scenario: Diagnostics collected
- **WHEN** a document references an unknown element type and an unresolved texture
- **THEN** the loader SHALL return an error diagnostic for the type and a warning diagnostic for the asset

