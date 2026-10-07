# editor-play-in-editor Specification

## Purpose
TBD - created by archiving change editor-play-in-editor. Update Purpose after archive.
## Requirements
### Requirement: Duplicate world for play
The editor SHALL run the game on a world **duplicated** from the edit world, so play never mutates the authored
world. The duplicate SHALL include the actors/components and the subsystem set (from the world description).

#### Scenario: Play duplicates
- **WHEN** the user starts Play
- **THEN** a runtime world SHALL be created as a copy of the edit world (actors/components + subsystems) and
  run, while the edit world is left intact

#### Scenario: Edit world is not ticked
- **WHEN** the game is playing
- **THEN** only the runtime world SHALL advance; the edit world SHALL NOT be ticked or mutated

#### Scenario: Subsystems run in the runtime world
- **WHEN** Play starts and the world enables a subsystem (e.g. physics)
- **THEN** the runtime world's subsystem SHALL be started, independently of the edit world's subsystems

### Requirement: Play / Pause / Stop
The editor SHALL provide Play, Pause, and Stop controls. Play runs the runtime world; Pause suspends it; Stop ends
the session and discards the runtime world.

#### Scenario: Pause and resume
- **WHEN** the user pauses and then plays again
- **THEN** the runtime world SHALL continue from where it paused (same world), not restart

#### Scenario: Stop discards
- **WHEN** the user stops
- **THEN** the runtime world SHALL be discarded and the editor SHALL return to editing the (unchanged) edit world

#### Scenario: Play without a world
- **WHEN** the user requests Play while no world document is open
- **THEN** no session SHALL start and the editor SHALL remain in the editing state

### Requirement: Play state is visible
The editor SHALL reflect the current play state (Editing / Playing / Paused) so the user can tell whether the
world is running.

#### Scenario: State surface
- **WHEN** the play state changes
- **THEN** the editor SHALL update its play-state indicator accordingly

### Requirement: Session does not outlive its document
Opening, creating, or closing a world SHALL stop any running play session first.

#### Scenario: Opening or closing a world during play
- **WHEN** the user opens, creates, or closes a world while a session is running
- **THEN** the running session SHALL be stopped before the document changes
