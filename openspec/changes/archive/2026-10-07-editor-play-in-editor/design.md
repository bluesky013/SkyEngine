## Context

`WorldDocument` (`engine/sandbox/core`) owns a `sky::World` (actors/components + subsystems built through the
`WorldSubSystemRegistry` from a serializable `WorldDesc`). `sky::World` already round-trips through
`SaveJson`/`LoadJson`, and `World::Build(WorldDesc)` recreates subsystems from the description. The host
(`SandboxModule`) does not tick any world today — there is no render/play loop yet, which keeps the initial PIE
surface small.

UE solves PIE by duplicating the world (`DuplicateWorldForPIE`). We mirror that using the serialization we
already have.

## Goals / Non-Goals

**Goals:** a runtime world duplicated from the edit world (actors/components **and** subsystems); Play/Pause/Stop
that never mutate the edit world; a visible play state; controls + shortcuts.

**Non-Goals:** rendering the play world (needs the render main loop); forwarding game input (needs the
viewport); persisting runtime state; Save/reload during play; a full editor toolbar (a menu + shortcuts first);
PIE-specific subsystem overrides (deferred).

## Decisions

1. **Duplicate by serialization (reuse the serializer).** `WorldDocument::CreatePlayWorld()` creates a fresh
   `World`, calls `Init()`, serializes the edit world to JSON and loads it into the copy (this carries actors,
   components and the `WorldDesc`), then calls `Build(*desc)` so the runtime subsystems are created from the
   same description. Rationale: avoids a hand-written deep copy of every component pool and reuses the tested
   round-trip. A native `World::Duplicate` can replace it later if profiling asks for it.

2. **Subsystem lifecycle.** `World` gains `StartSimulation()` / `StopSimulation()` that iterate its subsystems and
   call the matching `IWorldSubSystem` hook. Play calls `StartSimulation()` on the **runtime** world so physics /
   navigation run; Stop simply drops the runtime world (its destructor already calls `OnDetachFromWorld` on every
   subsystem). The edit world's subsystems are never started/ticked during play.

3. **Tick ownership (single owner).** The host `Tick(dt)` calls `PlaySession::Tick(dt)`. The runtime world is
   ticked **only** while `Playing`; the edit world is **not** ticked while playing (and is not ticked at all
   today). When a future edit-preview needs it, gate edit-world ticking on `state != Playing` so physics/nav are
   never double-run.

4. **Play session state machine (host-owned).** `PlaySession { State state; WorldPtr world; float time; }` with
   `Play/Pause/Stop/Tick`:
   - `Play` in `Editing` → duplicate + `StartSimulation`, state `Playing`.
   - `Play` in `Paused` → resume the **same** world, state `Playing` (no re-duplicate).
   - `Pause` in `Playing` → state `Paused`; ticking stops, the world is kept alive (pause ≠ teardown).
   - `Stop` in any state → discard the world → state `Editing`.
   `Play` is a no-op when no world document is open (and is logged).

5. **Edit world untouched / no dirty.** Nothing writes to the edit world during play; `Stop` only discards the
   runtime world. Play state is **not** part of the document dirty/undo state.

6. **Controls, shortcuts and state.** `EditorShell` exposes Play/Pause/Stop handlers and a `SetPlayState`
   surface. A **Play** menu holds Play/Pause/Stop; `F5` toggles Play/Pause and `Shift+F5` stops (UE convention).
   The status bar's mode indicator reflects `Editing` / `Playing` / `Paused`.

7. **Session invalidation.** Opening, creating, or closing a world (and quitting) stops any running session
   first, so a session never outlives the document it was duplicated from.

8. **Viewport seam.** `PlaySession::GetWorld()` (and `SandboxModule` exposure) returns the runtime world for a
   future viewport / render-scene bridge to consume. Not used in v1.

## Prior art (grounded)

UE: `Play In Editor` duplicates the world, with Play/Pause/Stop, a separate-process option, and Save-during-PIE
menus; the camera/selection are restored on Stop. Godot/Unity: a Play button enters a run mode over the current
scene. We take the **duplicate-not-mutate** core (UE) and the minimal Play/Pause/Stop UI, deferring the rest.

## Risks / Trade-offs

- **[Serialization-based duplication cost]** duplicating via JSON is heavier than a native copy; acceptable for
  v1, revisit with `World::Duplicate`.
- **[Shared UUIDs]** the duplicate keeps the edit world's actor UUIDs; safe while worlds are isolated, but any
  process-global registry keyed by actor UUID would collide — not used today.
- **[No viewport yet]** without scene rendering, PIE is observable only via world/subsystem ticks + editor state;
  the runtime world is kept ready for the viewport.
- **[Subsystem side effects]** starting physics/nav in the runtime world while the edit world exists is safe only
  because the edit world is not ticking; keep the single-owner rule (decision 3).
- **[No Save-during-PIE]** editing is live; changing the document while playing is not supported (Stop first).

## Decisions deferred

- Save/reload during play, per-subsystem play overrides, simulate-in-editor mode, play-from-here, input
  forwarding, and play-world rendering — all gated on the viewport/render path.
