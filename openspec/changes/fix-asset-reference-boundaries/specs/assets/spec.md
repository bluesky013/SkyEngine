## ADDED Requirements

### Requirement: Source identity includes owning mount
The source catalog SHALL preserve separate identities for equal relative paths in different mounts. UUID reads SHALL open the source from its owner. Explicit mount lookup SHALL bypass overlay precedence without changing unqualified lookup semantics.

#### Scenario: Project and Engine source paths collide
- **WHEN** Project and Engine both contain shapes/cube.glb with different manifest UUIDs
- **THEN** both identities survive scanning and UUID reads return each owner's bytes
- **AND** unqualified lookup selects Project while explicit Engine lookup selects Engine

#### Scenario: Cook order does not override source selection
- **WHEN** same-path Engine and Project sources publish products in either order
- **THEN** the product path index selects the overlay source and both UUID products remain independently loadable

#### Scenario: Authoring source overrides a stale product path
- **WHEN** an uncooked Project source overlays a previously cooked Engine path
- **THEN** AllowCook resolves the Project identity before consulting product indexes
- **AND** ProductsOnly continues to use the published index snapshot without consulting source catalogs

### Requirement: Manifest mutations preserve external commits
Every manifest read-modify-write SHALL acquire its filesystem lock and reread current persisted state before modifying entries. Multi-manifest operations SHALL order locks consistently and retain them through recoverable rollback.

#### Scenario: Editor cache predates worker commit
- **WHEN** a worker publishes generated products or cook settings after the editor cached the directory manifest and the editor moves or removes another source
- **THEN** the worker's committed entries and UUIDs remain unchanged

### Requirement: Source deletion persists across catalog rebuild
Source deletion SHALL remove the source file and its manifest identity before reporting success, reject read-only sources, and return explicit failure when persistence fails. Catalog-only unregister SHALL remain distinct.

#### Scenario: Rescan after editor deletion
- **WHEN** a writable source is successfully deleted and the source catalog is rebuilt
- **THEN** neither the original identity nor a replacement identity is discovered for that file

#### Scenario: Failed source deletion
- **WHEN** source removal or manifest persistence fails recoverably
- **THEN** failure is reported and recoverable original file and identity state is preserved
