## ADDED Requirements

### Requirement: Engine builtin configuration has one authoritative location
Repository-level engine builtin configuration SHALL live in the repository-root `configs/` directory. A duplicate `engine/configs/` tree SHALL NOT exist.

#### Scenario: Only the root configs directory exists
- **WHEN** the repository layout is inspected
- **THEN** engine builtin configuration files SHALL be present under `configs/`, and `engine/configs/` SHALL NOT exist

#### Scenario: Duplicate file removed
- **WHEN** the former `engine/configs/mesh_build_presets.json` is compared with the root file
- **THEN** the root `configs/mesh_build_presets.json` SHALL remain unchanged and the duplicate SHALL be removed

### Requirement: Build and packaging consume the authoritative configs directory
CMake configuration, editor and game deployment, and Python packaging SHALL read engine builtin configuration from the repository-root `configs/` directory.

#### Scenario: Editor deployment reads root configs
- **WHEN** `SandboxEditor` is built
- **THEN** its deployed `configs/` SHALL be copied from the repository-root `configs/`

#### Scenario: Game deployment reads root configs first
- **WHEN** a generated game project deploys configuration
- **THEN** it SHALL deploy the repository-root engine `configs/` first and the project `configs/` second, preserving project override precedence

#### Scenario: Packaging reads root configs
- **WHEN** `python/package_project.py` stages configuration
- **THEN** it SHALL copy from the repository-root `configs/` and the project `configs/` in that order

#### Scenario: Editor module manifest resolves
- **WHEN** top-level CMake reads the editor module manifest
- **THEN** the path SHALL resolve to the repository-root `configs/modules_editor.json`
