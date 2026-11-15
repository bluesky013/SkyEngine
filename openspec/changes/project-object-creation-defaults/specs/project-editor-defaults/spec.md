## ADDED Requirements

### Requirement: Project-owned creation settings
The editor SHALL store project-shared object creation defaults in configs/editor.json and edit them using reflected controls, isolated working values and undoable commands.

#### Scenario: Apply project defaults
- **WHEN** a user applies valid project creation defaults
- **THEN** the project file is saved and subsequent creations copy those defaults without modifying existing objects or user preferences

#### Scenario: Invalid configuration
- **WHEN** a configuration is malformed or the selected material asset is missing or incompatible
- **THEN** the editor reports the failure without silently substituting another material or overwriting valid settings

### Requirement: Repository-backed object creation
Cube and Plane creation SHALL resolve engine repository mesh sources through the asset database. Components SHALL retain their existing mesh and material UUID fields.

#### Scenario: Primitive creation
- **WHEN** a Cube or Plane is created
- **THEN** the object references shapes/cube.glb or shapes/ground_plane.glb and copies the material UUID from project defaults

#### Scenario: Missing mesh source
- **WHEN** the required mesh source cannot be resolved
- **THEN** creation fails without adding an empty actor or recording an undo operation

#### Scenario: Default material
- **WHEN** no project configuration exists
- **THEN** the Aurora editor default references the repository materials/lit.mat source, cooked and loaded through the normal asset pipeline

### Requirement: Stable editing scope
The editor SHALL keep project defaults separate from user preferences and SHALL discard unapplied working edits on cancellation or project teardown.

#### Scenario: Cancel project settings
- **WHEN** the user cancels the settings dialog
- **THEN** unapplied edits do not affect the project file or subsequent object creation

### Requirement: Existing scene compatibility
The change SHALL preserve existing UUID-based scene serialization and legacy resource resolution.

#### Scenario: Existing scene
- **WHEN** an existing scene is loaded and saved
- **THEN** its stored mesh and material references remain unchanged unless explicitly edited

### Requirement: Engine cook configuration defaults
Asset cooking SHALL use project configuration files when present and engine configuration files otherwise. Invalid explicit cook configuration SHALL report failure instead of silently selecting defaults.

#### Scenario: Project without local cook presets
- **WHEN** an engine mesh or material is loaded in a project without local cook configuration
- **THEN** engine presets and the registered output bundles support the normal cook/load pipeline

#### Scenario: Project overrides engine presets
- **WHEN** the project has a cook configuration under the supported legacy filename
- **THEN** that file takes priority over the engine configuration even if the engine uses the newer filename

### Requirement: Preserve asset display and hover hints
Both repository asset references and legacy references adopted by engine asset manifests SHALL retain human-readable display paths and hover hints through editor session restarts.

#### Scenario: Inspect a legacy scene resource
- **WHEN** the inspector displays a legacy Cube, Plane, Lit or Unlit reference
- **THEN** it shows the Engine source path instead of a bare GUID and its hover hint includes the path, type and GUID

### Requirement: One engine asset namespace and load pipeline
The four legacy primitives/materials SHALL be real engine repository assets with stable manifest identities, without Builtin virtual catalog entries or special runtime GUID generation.

#### Scenario: Legacy scene load and save
- **WHEN** a scene references the former Cube, Plane, Lit or Unlit identities
- **THEN** ordinary asset resolution loads the corresponding engine files, preserves mesh dimensions and material behavior, and save includes those asset dependencies

#### Scenario: Repository asset unavailable
- **WHEN** the required engine asset cannot be loaded
- **THEN** the caller reports the missing asset rather than generating a replacement by GUID

### Requirement: Preserve source ownership during mutations
Asset database mutations SHALL reject read-only mounts and preserve registered identities when manifest writes fail.

#### Scenario: Mutate an Engine source directly
- **WHEN** a caller removes or moves an Engine source through AssetDataBase
- **THEN** the operation fails without changing source files, manifest identities or catalog records

#### Scenario: Failed writable move
- **WHEN** a manifest write fails after the source file was renamed
- **THEN** the move restores the previous source location and committed destination manifest, retains the cached identity, and reports any recovery failure

### Requirement: Mesh products own fallback dependencies
Mesh cookers SHALL publish complete material slots and deduplicated material dependencies in their product headers. Component serialization SHALL collect explicit component references without global fallback lookups.

#### Scenario: Unspecified material slots
- **WHEN** a mesh is cooked without supplied material slots
- **THEN** its slots reference the repository Unlit material and the product dependency list includes that material, or cooking fails explicitly if the fallback is unavailable

#### Scenario: Malformed project file
- **WHEN** loading or saving encounters malformed project settings
- **THEN** the original file is preserved and the editor instructs the user to repair the file and restart, rather than recommending an Apply that cannot succeed
