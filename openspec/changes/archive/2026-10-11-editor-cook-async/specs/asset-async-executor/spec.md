## ADDED Requirements

### Requirement: Asset event callbacks are delivered on the main thread

Any `IAssetEvent` broadcast that can originate off the main thread SHALL be posted to the
`ThreadRole::Main` queue (explicitly, at the emit site) and SHALL run when the main role dispatches
(once per frame, before the module/editor tick), never on a worker. This covers
`OnAssetBuildFinished` (from the build worker and the out-of-process monitor) and `OnAssetLoaded`
(from the load worker).

#### Scenario: Build completion on the main thread
- **WHEN** an asset build completes on a worker thread
- **THEN** `OnAssetBuildFinished` SHALL be invoked on the main thread

#### Scenario: Asset loaded on the main thread
- **WHEN** an asset finishes loading on a worker thread
- **THEN** `OnAssetLoaded` SHALL be invoked on the main thread

#### Scenario: Dispatch before the tick
- **WHEN** the application frame begins
- **THEN** the main role's queue SHALL be dispatched before the module/editor tick

### Requirement: On-demand load completion is not deferred

The completion callback passed to a build request (used to resolve on-demand `LoadAsset` waits) SHALL
NOT be posted to the main role queue, because some waiters block the main thread (`BlockUntilLoaded`).
It SHALL run on the caller/worker thread so a main-thread wait cannot deadlock.

#### Scenario: Main-thread blocking load still returns
- **WHEN** the main thread calls `LoadAsset(...)->BlockUntilLoaded()` and the product must be built
- **THEN** the build's completion SHALL resolve the wait from the worker and the main thread SHALL
  return (no deadlock on the main queue)

### Requirement: Cook runs on a dedicated pool without losing dedup

In-process cook builds SHALL run on the dedicated cook pool, and moving them off the loader pool SHALL
preserve per-target deduplication (`uuid#target`) and the wait-for-all used before persisting state.

#### Scenario: Dedup preserved
- **WHEN** the same asset + target is requested more than once before it starts
- **THEN** exactly one build SHALL run

#### Scenario: Wait-for-all covers cook
- **WHEN** the database is saved while a cook is in flight
- **THEN** draining SHALL wait for in-flight cook tasks
