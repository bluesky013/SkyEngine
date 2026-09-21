## Context

Tile paging (`navigation-tile-streaming`) loads tiles synchronously inside the world tick; a burst of tiles in one
frame can spike frame time, and there is no cancellation. This change moves the per-tile payload preparation
off-thread and applies tiles on the main thread under a per-frame budget.

## Goals / Non-Goals

**Goals**
- Off-thread tile payload prefetch through the existing `Task` system.
- Main-thread-only tile application (Detour tile cache/nav mesh mutation is not thread-safe).
- Polled completion, cancellation, and per-frame budgeting.
- Failure/staleness handled without blocking gameplay.

**Non-Goals**
- Reading tiles from disk (the manifest is in memory); this is payload preparation, not file I/O.
- A frame-time profiling harness (verification task deferred).

## Decisions

### D1. Task-based prefetch
`NaviMeshTileLoadTask : Task` deep-copies a `NaviMeshTilePayload` in `DoWork` and exposes a polled `IsFinished()`.
`NavigationSystem` keeps `pendingLoads` keyed by tile coordinate.

### D2. Main-thread apply
`NavigationSystem::UpdateStreaming` never adds tiles directly from the gathering pass; it enqueues tasks, then
applies finished prefetches with `NaviMesh::AddTile` on the main thread.

### D3. Cancellation
A pending tile that moves beyond the unload radius (or when streaming is reset) has `ResetTask()` called and is
dropped, so stale requests do not apply.

### D4. Per-frame budget
`SetLoadBudget(tilesPerTick)` caps how many finished prefetches are applied per tick, bounding frame cost.

## Risks / Trade-offs

- [Task scheduling latency] -> prefetch is cheap (a copy); the budget, not the task, bounds frame time.
- [Manifest payload lifetime] -> tasks reference payloads owned by the nav mesh asset, which outlives the system.

## Open Questions

- Profiling the per-tick application cost in a real scene (task 3.2).
