## ADDED Requirements

### Requirement: Asset browser is a renderer

The asset browser panel SHALL render the catalog-built tree and item set and perform layout and hit
testing only; it SHALL NOT assemble the folder tree or query per-folder contents.

#### Scenario: Render core-built tree
- **WHEN** the panel displays the folder tree
- **THEN** it SHALL render the tree provided by the catalog without recursing per folder

#### Scenario: View holds no structure
- **WHEN** the catalog changes
- **THEN** the panel SHALL re-render from the catalog rather than maintaining its own folder model

### Requirement: Detail view shows targets and settings

The asset browser detail view SHALL show the active platform, the resolved cook targets, and, for
each target, the effective per-kind settings reported by the asset's builder (or the targets alone
when the kind reports no settings).

#### Scenario: Texture settings shown
- **WHEN** a texture is selected
- **THEN** the detail view SHALL show its targets and each target's effective settings (e.g. encode,
  srgb, max size, mip generation)

#### Scenario: No builder settings
- **WHEN** the asset's kind reports no settings
- **THEN** the detail view SHALL show the targets without settings and SHALL NOT error

### Requirement: Usable default bottom dock

The default layout SHALL give the bottom dock (asset browser + output log) a usable height, set
explicitly on the correct split rather than left to an accidental ratio.

#### Scenario: Bottom dock visible by default
- **WHEN** the editor starts with the default layout
- **THEN** the bottom dock SHALL show the asset browser with enough height to display items
