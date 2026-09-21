## Why

`NaviMesh::FindPath` is synchronous and runs on the calling thread against a single shared `dtNavMeshQuery`, and
its result is only a `SUCCESS`/`FAILED` status with no reusable path output. It cannot be used for crowd/gameplay
pathfinding without stalling frames, and `dtNavMeshQuery` is not safe for concurrent use. This change adds an
asynchronous query path plus the result and filter APIs that make queries actually usable.

## What Changes

- Add an asynchronous path request: submit `(start, end, filter, params)`, receive a handle, get the result via a
  callback or polled status.
- Run queries on worker threads with a query-instance pool (one `dtNavMeshQuery` per worker); never query from the
  main thread synchronously.
- Provide a real `NaviPath` result (ordered points + flags) and a `NaviQueryFilter` factory so callers can create
  filters instead of the backend static-casting an abstract pointer.
- Support cancellation and invalidate in-flight queries when the nav mesh changes
  (`NavigationSystem::OnNavMeshChanged`).
- Keep a synchronous variant for tools/tests where blocking is acceptable.

## Capabilities

### New Capabilities
- `navigation-async-query`: asynchronous path queries, query pooling, `NaviPath` result, and query-filter factory.

## Related backlog (to split into its own change when expanded)

- `navigation-agents`: DetourCrowd-based agents / local avoidance that would consume async queries.

## Impact

- `engine/navigation` (`NaviMesh` query API, `NaviPath`, `NaviQueryFilter`), `plugins/recast` (query
  implementation + pool), `engine/core/async/Task` reuse.

## Open Questions (expand later)

- API shape: `std::future`-style vs handle + callback; query budget/priority.
- Whether results return world-space points only or also polygon refs for partial/corridor queries.
