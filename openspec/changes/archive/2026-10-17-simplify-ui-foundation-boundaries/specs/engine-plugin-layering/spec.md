## ADDED Requirements

### Requirement: UI foundation implementation remains in its engine module
UI SHALL keep its runtime, render, optional FreeType and document-editor implementations in `engine/ui/{core,render,freetype,editor}`. This explicit foundation ownership SHALL retain independent targets and one-way dependencies toward runtime/contracts, with no UI feature-plugin registration. Existing target names and `ui/...` public includes SHALL remain available.

#### Scenario: Build consumes relocated foundation targets
- **WHEN** a host selects UI, optional FreeType or document editing
- **THEN** CMake SHALL use the matching engine/ui targets without requiring plugins/ui, and runtime SHALL NOT depend on editor/render/font implementations

#### Scenario: Renderer implementation is relocated
- **WHEN** UI rendering sources move back to the foundation
- **THEN** relocation SHALL preserve implementation bytes and SHALL NOT change renderer resources, submissions or backend behavior
