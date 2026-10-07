---
title: "Play-In-Editor (PIE)"
description: "Duplicate the edit world into a runtime world and run it with Play/Pause/Stop, leaving the authored world untouched."
updated: "2026-10-07"
---

## Scope

Play-In-Editor (PIE) runs the game on a **duplicated** copy of the edit world so Play/Pause/Stop never mutate
authored state. It covers:

1. **World duplication** — a runtime world copied from the edit world (actors/components + subsystems).
2. **A play session** — a `Editing` / `Playing` / `Paused` state machine that owns the runtime world.
3. **Editor controls** — a **Play** menu plus `F5` / `Shift+F5`, with the play state shown in the status bar.

Grounded in the code: `engine/sandbox/core` (`WorldDocument`, `PlaySession`), `engine/sandbox/module`
(`SandboxModule`), `engine/sandbox/shell` (`EditorShell`), `engine/framework` (`World`).

## World duplication

`WorldDocument::CreatePlayWorld()` builds a fresh `sky::World` from the edit world:

1. Create a world and `Init()` it.
2. Serialize the edit world (`World::SaveJson`) into an in-memory `StreamArchive` and `LoadJson` it into the
   copy — this carries the actors, their components, and the `WorldDesc`.
3. `Build(*desc)` so the runtime subsystems are created from the same description.

`PlaySession` owns the simulation lifecycle: `Play()` calls `world->StartSimulation()` and `Stop()` calls
`world->StopSimulation()` before discarding the world (duplication itself does not start it).

Rationale: it reuses the tested serialization round-trip instead of hand-copying every component pool. The edit
world is only read, never written.

`World` gained `StartSimulation()` / `StopSimulation()` (iterate subsystems and forward to the matching
`IWorldSubSystem` hook). Dropping a runtime world already detaches its subsystems via `~World`.

## Play session

`editor::PlaySession` (UI-free, in `editor/core`) owns the runtime world and sim time:

| State | Meaning |
|---|---|
| `Editing` | no runtime world; the editor is authoring |
| `Playing` | a runtime world exists and is ticked |
| `Paused` | a runtime world exists but is not ticked |

- `Play()` — from `Editing`, duplicates via the injected world factory; from `Paused`, resumes the **same**
  world (no re-duplication). Returns false when there is no source world.
- `Pause()` — stops advancing; the runtime world is kept alive.
- `Stop()` — discards the runtime world and returns to `Editing`.
- `Tick(dt)` — advances only while `Playing`.

The host (`SandboxModule::Tick`) drives `playSession.Tick(delta)` when not in hub mode. The edit world is never
ticked while playing, so physics/navigation are never double-run.

## Editor controls

- **Play** menu: `Play` / `Pause` / `Stop`.
- `F5` toggles Play/Pause; `Shift+F5` stops.
- `EditorShell::SetPlayState` reflects the state in the status bar mode (`Edit` / `Play` / `Pause`).
- Opening, creating, or closing a world (and quitting) stops any running session first.

With no world document open, `Play` is a no-op (`PlaySession::Play` returns false), so the editor stays in
`Editing`.

## Non-goals

- **Rendering the play world** — depends on the render main loop; PIE currently runs world logic + subsystem
  ticks only. `PlaySession::GetWorld()` is the seam a future viewport/render-scene bridge will consume.
- Forwarding player/game **input** (needs the viewport).
- Persisting runtime state, multiplayer, or a standalone build/player.
- Simulate-in-editor (running game logic inside the edit viewport).

## Tests

- `FrameworkTest.WorldSubSystemRegistryTest.StartStopSimulationIteratesSubsystems`.
- `EditorCoreTest.WorldDocumentTest.CreatePlayWorldDuplicates` — the edit world is unchanged and the copy has
  the same actors.
- `EditorCoreTest.PlaySessionTest.*` — state machine, play-without-world, tick-only-while-playing.
- `EditorShellTest.F5TogglesPlayPauseAndShiftStops`.
