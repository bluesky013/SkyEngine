## ADDED Requirements

### Requirement: Preparation before history traversal

CommandService SHALL provide removable UI-independent preparation callbacks before every Undo or Redo attempt, including an empty stack. Callbacks SHALL run in registration order before stack selection, and the service SHALL recheck the stack afterward. Removed callbacks SHALL NOT run later in the same preparation snapshot; newly added callbacks SHALL start with the next attempt. Recursive Undo/Redo SHALL return false during the outer preparation, command execution, and change notification.

#### Scenario: Prepare a live preview before Undo
- **WHEN** a caller requests Undo while a preparation observer owns an uncommitted preview
- **THEN** preparation SHALL finish before the command restores its previous value
- **AND** subsequent interaction completion SHALL NOT overwrite the restored value

#### Scenario: Empty or changed history
- **WHEN** the selected stack is empty initially or a preparation callback clears it
- **THEN** preparation SHALL still run and traversal SHALL return false without accessing a missing command

#### Scenario: Observer lifetime and recursion
- **WHEN** a preparation observer removes another observer or requests recursive Undo/Redo
- **THEN** the removed observer SHALL be skipped and recursive traversal SHALL return false
- **AND** the outer attempt SHALL preserve its intended single traversal

### Requirement: Cleared history generations

Clearing history SHALL invalidate the generation of an executing command. After a synchronous Do/Undo/Redo callback clears or replaces the history session, the service SHALL NOT push the old command into the current session's stacks. Commands added by the replacement session SHALL be preserved. An Undo/Redo whose command executed before Clear SHALL report true even though its old generation is no longer retained.

#### Scenario: Session changes during command notification
- **WHEN** command execution synchronously clears history and optionally creates a command in a replacement session
- **THEN** the old command SHALL NOT reappear in either stack
- **AND** the replacement session's commands SHALL remain independently undoable
