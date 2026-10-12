# Delta: editor-project-manager

## MODIFIED Requirements

### Requirement: Project descriptor

The project descriptor (`<name>.skyproj`) SHALL be the single source of project identity and settings. Creating a
project SHALL produce the directory layout **and** a compilable game source tree: `assets/`, `configs/`, `cache/`,
`<name>.skyproj`, plus `native/` (game module skeleton, platform launcher entry, independent build entry), with the
game module registered in `<name>.skyproj` `modules` and the project module list. Project creation SHALL be
implemented by the packaging CLI (`python/package_project.py create`); the editor SHALL invoke that CLI rather than
re-implementing scaffolding.

#### Scenario: Create a project

- **WHEN** a new project is requested (from the editor or the CLI)
- **THEN** a directory with `assets/`, `configs/`, `cache/`, `native/` (module + launcher + build entry), and `<name>.skyproj` SHALL be created, and the descriptor SHALL list the game module

#### Scenario: Existing projects still load

- **WHEN** an existing project without `native/` is opened
- **THEN** it SHALL still open (the scaffold is only added on creation; migration is not required)
