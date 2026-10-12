# Delta: editor-project-manager

## MODIFIED Requirements

### Requirement: Project descriptor
The project descriptor (`<name>.skyproj`) SHALL be the single source of project identity and settings. Creating a
project SHALL produce the directory layout **and** a compilable game source tree: `assets/`, `configs/`, `cache/`,
`<name>.skyproj`, plus `source/` (a game module skeleton) and a project build entry, with the game module
registered in `<name>.skyproj` `modules` and the project module list.

#### Scenario: Create a project

- **WHEN** a new project is requested
- **THEN** a directory with `assets/`, `configs/`, `cache/`, `source/`, a build entry, and `<name>.skyproj` SHALL be created, and the descriptor SHALL list the game module

#### Scenario: Existing projects still load

- **WHEN** an existing project without `source/` is opened
- **THEN** it SHALL still open (the scaffold is only added on creation; migration is not required)
