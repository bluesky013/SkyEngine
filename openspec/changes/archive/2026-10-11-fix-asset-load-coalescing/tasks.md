# Tasks

> Not started. Plan only (see proposal.md, design.md).

## 1. Atomic on-demand coalescing

- [x] 1.1 In `AssetManager::LoadAssetOnDemand`, hold `mutex` across find-or-create + dedup + promise creation + wait-handle assignment + `pendingJobs` registration; schedule the cook only after releasing the lock
- [x] 1.2 Dedup on the in-flight load (`pendingJobs[uuid]`), not on a separately-cleared `pendingCooks` set, so the guard and the wait handle share a lifetime
- [x] 1.3 Re-check the asset status under the lock and return early when already LOADED (no redundant second cook); allow re-attempt when FAILED
- [x] 1.4 Never overwrite an existing in-flight wait handle

## 2. Synchronize the wait handle

- [x] 2.1 Store `AssetBase::asyncTask.second` as `std::shared_future<void>` (copyable)
- [x] 2.2 `BlockUntilLoaded`: copy the handle under the lock, then wait outside the lock (never hold the lock while waiting)

## 3. Tests

- [x] 3.1 Add a stress regression: many threads (e.g. 8) each load the same unbuilt UUID over many iterations; all `BlockUntilLoaded()` must return and exactly one cook is scheduled per load
- [x] 3.2 Keep `ConcurrentLoadTest` green when run repeatedly (the previous intermittent hang must not reproduce)

## 4. Performance + verification

- [x] 4.1 Confirm coalescing is preserved (N concurrent loads -> 1 cook) and the load critical section stays O(1) (no I/O/cook/wait under the lock)
- [x] 4.2 Build both configs; run `FrameworkTest` (including the new stress test) repeatedly with no hang
