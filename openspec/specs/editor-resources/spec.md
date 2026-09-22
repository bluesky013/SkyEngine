# editor-resources Specification

## Purpose
TBD - created by archiving change sandbox-editor-resources. Update Purpose after archive.
## Requirements
### Requirement: Editor builtin resource root

The sandbox editor SHALL own a builtin resource tree for editor-only resources (icons, editor fonts) at
`engine/sandbox/resources/`, separate from the project-package `assets/` tree.

#### Scenario: Resource root contents
- **WHEN** the editor resource tree is inspected
- **THEN** it SHALL contain the editor's builtin resources (for example `icons/` and `fonts/`) under a single
  root that is not the project package `assets/`

### Requirement: Bundle-relative resource resolution

The editor SHALL resolve builtin resource paths relative to the executable (platform bundle path), not the current
working directory, so the editor can be launched from any working directory.

#### Scenario: Resolve from a non-repo working directory
- **WHEN** the editor is launched from the executable's output directory (no `assets/` present)
- **THEN** `SandboxResources::Root()` SHALL resolve to `<executable_dir>/resources/` and `Resolve(relative)` SHALL
  return an absolute path under it

#### Scenario: No working-directory dependency
- **WHEN** the same resource is requested under different working directories
- **THEN** the resolved path SHALL be the same

### Requirement: Resource deployment to the executable

The build SHALL deploy the editor builtin resources next to the executable, preferring a symbolic link and falling
back to a recursive copy when symlinks are unavailable, without deleting the source tree.

#### Scenario: Symlink when supported
- **WHEN** the platform allows creating a directory symlink
- **THEN** the deployed `<executable_dir>/resources` SHALL be a symbolic link to the source resource tree

#### Scenario: Copy fallback
- **WHEN** creating the symlink fails (for example Windows without developer mode)
- **THEN** the deployment SHALL fall back to copying the resource tree into `<executable_dir>/resources`

#### Scenario: Source tree preserved
- **WHEN** a previous deployment (symlink or copied tree) is replaced
- **THEN** the source resource tree SHALL remain intact

### Requirement: Editor font loads from the builtin resources

The editor text system SHALL load its font through the builtin resource resolution, so the font is found when the
editor runs from the deployed output directory.

#### Scenario: Font found after deployment
- **WHEN** the editor starts from the output directory after a build
- **THEN** the font SHALL load from `<executable_dir>/resources/fonts/` and the text system SHALL NOT fall back to
  the block-glyph provider

