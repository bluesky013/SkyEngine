## 1. World duplication + subsystem lifecycle

- [x] 1.1 `World`: add `StartSimulation()` / `StopSimulation()` that iterate subsystems and call the matching
  `IWorldSubSystem` hook.
- [x] 1.2 `WorldDocument::CreatePlayWorld()`: create a fresh `World`, `Init`, load the edit world's serialized
  JSON into it (actors/components + `WorldDesc`), then `Build(*desc)`. Simulation start/stop is owned by
  `PlaySession` (not by duplication).
- [x] 1.3 Test (`FrameworkTest` or `EditorCoreTest`): after `CreatePlayWorld` the edit world is unchanged, the
  copy has the same actors/components, and its subsystems are present.

## 2. Play session

- [x] 2.1 `PlaySession` (module): `State { Editing, Playing, Paused }`, owns the runtime world + sim time,
  `Play/Pause/Stop/Tick(dt)`. (Lives in `editor/core` for testability.)
- [x] 2.2 Semantics: Play duplicates (Editing) or resumes (Paused); Pause stops advancing but keeps the world;
  Stop discards the world; Play with no document is a no-op.
- [x] 2.3 Host `Tick(dt)` drives the session; the edit world is not ticked while playing.
- [x] 2.4 Opening / creating / closing a world (and quitting) stops the session first.

## 3. Editor controls and state

- [x] 3.1 `EditorShell`: Play/Pause/Stop handlers + a `SetPlayState` surface; a **Play** menu; `F5` toggles
  Play/Pause and `Shift+F5` stops.
- [x] 3.2 Module wires the controls to `PlaySession` and reports state back; the status bar mode reflects
  `Editing` / `Playing` / `Paused`.
- [x] 3.3 Test: F5 with no document does not start a session; with a document the state toggles.

## 4. Docs and verification

- [x] 4.1 Document PIE in `docs/` (world duplication, subsystem lifecycle, play state, non-goal viewport).
- [ ] 4.2 Manual verify: Play runs the runtime world, Pause/Resume continues, Stop leaves the edit world
  unchanged.
