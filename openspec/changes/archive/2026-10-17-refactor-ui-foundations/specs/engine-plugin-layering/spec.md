## MODIFIED Requirements

### Requirement: Engine modules expose interfaces and data only
An engine feature's contract target SHALL contain only interface and referenced data definitions, and pure functions over that data; it SHALL NOT contain or depend on the feature's concrete implementation. Existing engine foundation modules, including UI, SHALL retain their established engine ownership and MAY separate concrete implementations into targets within the same `engine/<foundation>` module according to responsibility. Foundation ownership SHALL NOT weaken contract-target isolation: `UI.Interface` SHALL remain contracts, referenced data and pure utilities only, without including or linking UI implementation targets. This change SHALL NOT alter the established ownership of unrelated features.

#### Scenario: Engine contract target has no implementation
- **WHEN** an engine feature's contract target is inspected
- **THEN** it SHALL expose interfaces, referenced data types and pure utilities only, with no concrete feature implementation or dependency on implementation targets

#### Scenario: Existing foundation keeps separated implementations in engine
- **WHEN** an existing engine foundation such as UI separates runtime, editor and adaptor responsibilities into targets
- **THEN** its concrete implementation SHALL be allowed within its engine module while its contract target remains independent of those implementations

### Requirement: Features are implemented as plugins
A domain feature SHALL by default provide its concrete implementation in a plugin under `plugins/`, which implements its engine contract. Existing engine foundations such as UI SHALL be allowed to retain concrete implementations in their established engine module through separated targets and SHALL NOT require a feature-plugin entry merely to satisfy this default. This foundation rule SHALL NOT relocate unrelated feature implementations or alter existing domain-specific ownership requirements.

#### Scenario: Plugin implements the engine interface
- **WHEN** a domain feature plugin is loaded
- **THEN** it SHALL provide the concrete implementation registered with the engine interface

#### Scenario: Engine does not depend on a plugin
- **WHEN** an engine module is compiled and linked
- **THEN** it SHALL NOT link or include a plugin's implementation

#### Scenario: Foundation implementation does not require a plugin
- **WHEN** UI foundation implementation targets are configured within `engine/ui`
- **THEN** those targets SHALL NOT require a UI feature-plugin entry, and unrelated feature ownership SHALL remain unchanged

## ADDED Requirements

### Requirement: UI engine contracts are separate from toolkit implementation
UI SHALL be an explicitly authorized engine foundation exception: its implementation SHALL remain within `engine/ui`, separated by target responsibility. The root `UI.Interface` target SHALL contain only provider interfaces, referenced data, render-agnostic draw/layout/property data and pure utilities. Concrete retained elements, widgets, input routing, document schema/loading/bindings, text runtime, animation and localization SHALL live in `engine/ui/core`; document editing in `engine/ui/editor`; Aurora rendering in `engine/ui/render`; and the optional FreeType provider in `engine/ui/freetype`. `UI.Interface` SHALL NOT include or link those implementation targets. UI SHALL NOT require a feature-plugin entry, and this exception SHALL NOT alter ownership rules for unrelated features.

#### Scenario: Contract target stays independent within the foundation
- **WHEN** root `UI.Interface` source, public headers and target dependencies are inspected
- **THEN** that target SHALL contain only contracts/data/pure utilities and SHALL NOT include widget, document-loader, renderer, document-editor or font-rasterizer implementation

#### Scenario: UI foundation dependencies point toward core and interfaces
- **WHEN** the UI foundation target dependencies are inspected
- **THEN** editor and adaptor targets SHALL depend toward UI runtime or `UI.Interface`, and UI runtime SHALL NOT depend on editor or adaptor targets

#### Scenario: Public domain include paths survive relocation
- **WHEN** a consumer uses the separated targets under the `engine/ui` foundation
- **THEN** consumers linking the appropriate runtime, editor, or adaptor target SHALL retain their `ui/...` public includes

#### Scenario: Foundation implementation is allowed in engine UI
- **WHEN** the UI foundation's placement is reviewed against the general feature layering rule
- **THEN** `core`, `editor`, `render` and `freetype` implementations SHALL be allowed within `engine/ui`, while their one-way target dependencies remain enforced
