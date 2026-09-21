## Context

`NaviMesh::FindPath` is synchronous, returns only a status, and shares one non-thread-safe `dtNavMeshQuery`. It
cannot be used for gameplay pathfinding without stalling frames, and callers cannot obtain a filter or the path
points. This change adds a path result type, a filter factory, and an asynchronous query path.

## Goals / Non-Goals

**Goals**
- `NaviPath` result (world-space points + straight-path flags).
- `NaviQueryFilter` creation path through `NaviMeshFactory`.
- Synchronous `QueryPath` for tools/tests; `FindPath` kept as a status-only wrapper.
- Async path requests via `Task`, polled results, cancellation, and mesh-change invalidation.
- Safe worker-thread queries (serialized `dtNavMeshQuery` access).
- A concurrent-request budget.

**Non-Goals**
- DetourCrowd agents (`navigation-agents`).
- Per-thread query pooling (serialized access is used instead); profiling (task 3.2).

## Decisions

### D1. NaviPath result
`NaviPath` carries `result`, ordered `points`, and `flags`; `NaviMesh::QueryPath(..., NaviPath&)` fills it.
`FindPath` is re-expressed as `QueryPath` into a throwaway path, so there is one implementation.

### D2. Filter factory
`NaviMeshFactory::Impl` gains `CreateQueryFilter()` and the factory exposes `CreateQueryFilter()`; the recast
backend returns a `RecastQueryFilter`. Callers no longer static-cast an abstract filter.

### D3. Async query via Task
`NaviPathQueryTask : Task` runs `QueryPath` off the world tick and exposes a polled `IsFinished()` plus the path.
`NavigationSystem::RequestPath` submits tasks and prunes finished/cancelled ones each `Tick`.

### D4. Safe concurrent access
`RecastNaviMesh` guards `navQuery` with a mutex around `QueryPath`, so worker-thread queries are serialized rather
than racing. Query pooling is a future optimization.

### D5. Cancellation and invalidation
`CancelPath` flags a task; `NavigationSystem::OnNavMeshChanged` cancels and clears all in-flight queries so stale
results are not applied to a rebuilt mesh. `RequestPath` returns null when the concurrent budget is exhausted.

## Risks / Trade-offs

- [Serialized queries limit throughput] -> acceptable correctness-first choice; pooling is a follow-up.
- [Async tests need a built nav mesh] -> deferred to an integration harness (tasks 4.x).

## Open Questions

- Query pooling per worker vs a single serialized query under higher load.
