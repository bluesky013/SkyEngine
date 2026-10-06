# editor-project-manager Specification

## Purpose
TBD - created by archiving change editor-project-manager. Update Purpose after archive.
## Requirements
### Requirement: Editor startup modes

`SandboxEditor` SHALL open the Project Manager (hub) when no `--project` is given, and SHALL open the
editor bound to that project when `--project <path>` is given.

#### Scenario: No project opens the hub

- **WHEN** the editor is launched with no `--project`
- **THEN** it SHALL show the Project Manager instead of the editor shell

#### Scenario: Project opens the editor

- **WHEN** the editor is launched with `--project <path>`
- **THEN** it SHALL read the project descriptor and open the editor bound to it

### Requirement: Project descriptor

The editor SHALL read and write a JSON project descriptor (`*.skyproj`) carrying at least `id`, `name`,
`engineVersion`, and `defaultScene`, and SHALL be able to create a new project directory (with `assets/`,
`configs/`, `cache/`) and descriptor.

#### Scenario: Read a descriptor

- **WHEN** a valid `.skyproj` is opened
- **THEN** its fields SHALL be available to the editor (name shown; engine version validated)

#### Scenario: Create a project

- **WHEN** a new project is requested
- **THEN** a directory with `assets/`, `configs/`, `cache/`, and `<name>.skyproj` SHALL be created

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

### Requirement: Recent projects persistence

The recent-project list SHALL be persisted under the user config path and restored on the next launch.

#### Scenario: Recent list survives restart

- **WHEN** a project is added and the editor is relaunched
- **THEN** the project SHALL appear in the recent list

### Requirement: Engine-version validation

On open/add the editor SHALL compare the project's `engineVersion` with the running engine and SHALL
refuse a project newer than the engine, allowing an older one with a warning.

#### Scenario: Newer project refused

- **WHEN** the project's engine version is newer than the engine
- **THEN** the open/add SHALL be refused with a message

### Requirement: Single-instance hard lock

The editor SHALL hold a hard lock per project (`<project>/cache/editor.lock`) and SHALL refuse a second
editor on the same project while the owner process is alive; a stale lock (owner gone) SHALL be
reclaimed.

#### Scenario: Second instance refused

- **WHEN** a second editor opens a project already held by a live process
- **THEN** it SHALL refuse and exit

#### Scenario: Stale lock reclaimed

- **WHEN** the lock's owner process is gone
- **THEN** a new editor SHALL reclaim the lock and open

### Requirement: Project asset mounting

Opening a project SHALL mount the engine bundle assets read-only and the workspace `assets/` writable
(the workspace shadowing the engine), and SHALL set the runtime work filesystem to the project directory.

#### Scenario: Workspace shadows engine

- **WHEN** the same asset path exists in the workspace and the engine bundle
- **THEN** the workspace asset SHALL resolve

### Requirement: Console control

The editor SHALL run as a GUI-subsystem application with no console by default and SHALL attach a
console (stdout/stderr) when `--console` is passed.

#### Scenario: Default no console

- **WHEN** the editor is launched without `--console`
- **THEN** no console window SHALL open

#### Scenario: `--console` attaches a console

- **WHEN** the editor is launched with `--console`
- **THEN** a console SHALL be attached and logs SHALL appear on it

### Requirement: No standalone preview window

The editor SHALL NOT open a separate standalone preview window by default (the preview is a docked panel
in the target design).

#### Scenario: No preview window

- **WHEN** the editor opens (hub or a project)
- **THEN** no separate preview window SHALL be created

