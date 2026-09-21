## ADDED Requirements

### Requirement: Render- and toolkit-independent core module
The editor core SHALL live in `engine/sandbox/core` and build the `EditorCore` target as a static library that
links only `Core` and `Framework`. It SHALL build independently of the legacy Qt editor (`engine/editor`), which
is not modified. No public header under `include/editor/core/` SHALL include an `aurora/`, `render/`, or `ui/`
header, or any Qt header.

#### Scenario: Core builds without Qt
- **WHEN** the project is configured with `SKY_BUILD_SANDBOX` or `SKY_BUILD_TEST` on and Qt absent
- **THEN** the `EditorCore` target SHALL still be configured and SHALL build

#### Scenario: Forbidden dependency rejected
- **WHEN** a public header under `include/editor/core/` includes `aurora/`, `render/`, `ui/`, or a Qt header
- **THEN** configuration SHALL fail with an explicit error naming the offending header

### Requirement: Headless core services
The core services (undo/redo, property model, documents, selection) SHALL be usable without a window, a render
device, or any UI toolkit.

#### Scenario: Services run headless
- **WHEN** a test constructs and exercises a core service without initializing the platform or a renderer
- **THEN** the service SHALL operate and return results without requiring a window or GPU

### Requirement: Toolkit-independent extension registration
Editor extensions SHALL register asset creators, actor creators, gizmo factories, and inspectors through an API
whose contracts reference no UI toolkit type.

#### Scenario: Extension registers without Qt
- **WHEN** an extension module registers a creator
- **THEN** the registration SHALL compile and link without Qt and the registered factory SHALL be retrievable by
  the editor core
