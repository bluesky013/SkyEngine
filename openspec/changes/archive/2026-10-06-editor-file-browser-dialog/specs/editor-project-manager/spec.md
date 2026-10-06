## MODIFIED Requirements

### Requirement: Project Manager hub actions
The Project Manager hub SHALL let the user add an existing project, create a new project, open a selected
project, remove a project from the recent list, and delete a project folder. Creating a new project SHALL
use the file browser dialog in SelectDirectory mode to choose the target directory and a name; adding or
opening a project SHALL use the file browser dialog in OpenProject mode filtered to `*.skyproj`.

#### Scenario: New project chooses a directory
- **WHEN** the user activates New Project
- **THEN** the file browser dialog SHALL open in SelectDirectory mode, and on accept the project SHALL be
  created at the chosen directory with the entered name

#### Scenario: Add project chooses a file
- **WHEN** the user activates Add Project
- **THEN** the file browser dialog SHALL open in OpenProject mode, and on accept the chosen `*.skyproj`
  SHALL be validated and added to the recent list

#### Scenario: Invalid or cancelled selection
- **WHEN** the user cancels the dialog or selects an invalid project
- **THEN** no project SHALL be created or added, and the hub SHALL remain unchanged
