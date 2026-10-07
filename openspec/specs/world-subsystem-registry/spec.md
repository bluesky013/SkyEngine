# world-subsystem-registry Specification

## Purpose
TBD - created by archiving change world-subsystem-registry. Update Purpose after archive.
## Requirements
### Requirement: Subsystem factory registry
The engine SHALL provide a `WorldSubSystemRegistry` where subsystem plugins register, under a stable name, a
factory plus an optional **reflected config type**, an optional **default config** instance, and an optional
**validator** that reports a reason, so worlds can create subsystems without hosts depending on concrete types.
Re-registering a name SHALL override the previous registration (logged).

#### Scenario: Register and create
- **WHEN** a plugin registers a factory under a name and a world requests that name
- **THEN** the registry SHALL create and return the subsystem for that world

#### Scenario: Re-registration overrides
- **WHEN** a name is registered again
- **THEN** the new registration SHALL replace the previous one and the override SHALL be logged

#### Scenario: Unknown subsystem
- **WHEN** a world requests a name that is not registered
- **THEN** creation SHALL yield no subsystem (and report the miss)

#### Scenario: Declared config type and default
- **WHEN** a registration declares a config type
- **THEN** the registry SHALL expose that type and a default config instance the editor can use

#### Scenario: No config
- **WHEN** a subsystem is registered without a config type
- **THEN** it SHALL build with an empty config and use its own defaults

#### Scenario: Config validation
- **WHEN** a config is rejected by the subsystem's validator (which supplies a reason)
- **THEN** in develop/debug builds the build SHALL assert, and in release builds the entry SHALL be skipped,
  the reason logged, and the remaining subsystems SHALL still be built

### Requirement: Declarative world construction
A world SHALL be constructible from a `WorldDesc` that lists enabled subsystems (ordered) with per-subsystem
config, instead of hosts hardcoding concrete `new` calls. Subsystems SHALL be created through the registry and
attached in the described order; lifecycle hooks SHALL fire as today.

#### Scenario: Build from a description
- **WHEN** a world is built from a `WorldDesc` enabling physics and render scene
- **THEN** those subsystems SHALL be created via the registry, attached in order, and resolvable by name

#### Scenario: Disabled entry
- **WHEN** a `WorldDesc` entry is disabled
- **THEN** that subsystem SHALL NOT be created

#### Scenario: Manual attach still works
- **WHEN** a host attaches a subsystem explicitly
- **THEN** it SHALL behave as today (world owns it; resolvable by name)

### Requirement: Project-level world subsystem configuration (Sandbox editor)
The **Sandbox** editor SHALL expose a **project-level** world configuration surface (separate from user
preferences) listing the registered subsystems and letting the user enable/disable each and edit its
configuration; the selection SHALL be stored in and serialized with the project/world document.

#### Scenario: List subsystems
- **WHEN** the user opens the project's world configuration in the Sandbox editor
- **THEN** the editor SHALL list the registry-known subsystems with their enabled state

#### Scenario: Separate from user preferences
- **WHEN** the user edits a subsystem's configuration
- **THEN** it SHALL be stored with the project/world document, not in the user `Preferences` file

#### Scenario: Sandbox only
- **WHEN** the world subsystem configuration is implemented
- **THEN** it SHALL target the Sandbox editor; the deprecated Qt `engine/editor` SHALL NOT be modified

#### Scenario: Edit a subsystem's config
- **WHEN** the user selects a subsystem that declares a config type
- **THEN** the editor SHALL render its config fields (via reflection) from the default or stored instance and
  write edits back to the world's stored description

#### Scenario: Persist selection
- **WHEN** the user disables a subsystem and saves the world
- **THEN** the world's stored description SHALL reflect the change and rebuild accordingly on load
