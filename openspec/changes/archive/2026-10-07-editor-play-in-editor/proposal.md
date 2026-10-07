## Why

The editor edits the project world **in place**; there is no way to run it without mutating the authored state.
Industry editors provide **Play-In-Editor (PIE)**: run the game on a **duplicated** world so Play/Pause/Stop do
not touch the edit world (UE `DuplicateWorldForPIE`). This is the standard foundation for any "play" workflow and
must exist before scene rendering / gameplay iteration.

## What Changes

- **World duplication**: create a runtime copy of the edit world (actors/components + subsystems) without
  sharing mutable state with the edit world. Reuses the existing `World::SaveJson/LoadJson` + `World::Build`
  round-trip (no hand-written deep copy).
- **Play session**: a host-owned session (`Editing` / `Playing` / `Paused`) that owns the runtime world, advances
  it only while playing, and discards it on stop. The **edit world is never ticked/mutated** during play.
- **Editor controls**: Play / Pause / Stop actions (a **Play** menu + shortcuts) and a play-state indicator in
  the status bar.

## Capabilities

### New Capabilities
- `editor-play-in-editor`: duplicate the edit world into a runtime world and run it with play/pause/stop, leaving
  the edit world unchanged.

### Modified Capabilities
- *(none — additive on top of the world/document model. `World` gains `StartSimulation`/`StopSimulation`
  iteration helpers; `WorldDocument` gains `CreatePlayWorld`.)*

## Non-goals

- Full **game viewport rendering** in the editor (depends on the render main loop; PIE here runs world logic +
  subsystem ticks). The play world is exposed for the viewport to consume when ready.
- Forwarding player/game **input** to the play world (needs the viewport).
- Persistent runtime state (Play/Save/Pause menus in UE), multiplayer, build/standalone players.
- Simulate-in-editor (running game logic inside the edit viewport) — a later mode.

## Impact

- `engine/framework` (`World`): `StartSimulation()` / `StopSimulation()` iterate subsystems; used to start the
  runtime world's subsystems (and usable by any host).
- `engine/sandbox/core` (`WorldDocument`): `CreatePlayWorld()` builds a fresh world from the edit world's
  serialized JSON + `WorldDesc`.
- `engine/sandbox/module` (`SandboxModule`): a `PlaySession` (state machine + runtime world + `Tick`) driven from
  the host `Tick`.
- `engine/sandbox/shell` (`EditorShell`): Play/Pause/Stop actions + shortcuts + play-state surface (status bar).
