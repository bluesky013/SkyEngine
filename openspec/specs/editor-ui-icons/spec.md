# editor-ui-icons Specification

## Purpose
TBD - created by archiving change editor-ui-icons. Update Purpose after archive.
## Requirements
### Requirement: SVG icon rasterization through the DDC

The editor SHALL provide a UI icon builder that converts SVG source bytes into RGBA8 pixels at a
requested pixel size, registered with the derived data cache under a stable id so that results are
cached and reused across runs.

#### Scenario: Icon rasterized and cached

- **WHEN** the icon builder builds an SVG at a requested pixel size
- **THEN** it SHALL emit RGBA8 pixels at that size and the derived data cache SHALL store the result

#### Scenario: Requested size is part of the cache key

- **WHEN** the same SVG is built at two different requested sizes
- **THEN** each size SHALL be stored as its own cache entry

#### Scenario: Invalid source fails gracefully

- **WHEN** the source bytes are empty or cannot be parsed as SVG
- **THEN** the builder SHALL report failure without emitting pixels

### Requirement: Resource-backed icon bake in the editor

The editor SHALL bake UI icons from SVG sources stored under the sandbox resources tree and SHALL
store the derived output under the sandbox resources directory, so a control bound to a resource
icon renders from the cache; when the source cannot be read it SHALL fall back to a directly
generated glyph.

#### Scenario: Icon baked from a resource SVG

- **WHEN** the editor displays a control bound to `resources/icons/<name>.svg`
- **THEN** the icon SHALL be rasterized through the derived data cache and the derived bytes SHALL be stored under the sandbox resources directory

#### Scenario: Missing source still draws an icon

- **WHEN** the source SVG cannot be read
- **THEN** the control SHALL draw a directly generated icon without failing

#### Scenario: Icon control is interactive

- **WHEN** the pointer moves over or presses the icon control
- **THEN** the control SHALL show hover and press feedback in response

### Requirement: Icon dependency isolation

The SVG rasterization dependency SHALL be referenced only by the sandbox editor module and SHALL NOT
be exposed to engine or other plugin targets.

#### Scenario: Engine targets do not link the SVG rasterizer

- **WHEN** the engine and plugin target link dependencies are inspected
- **THEN** no engine target SHALL link the SVG rasterizer third-party

