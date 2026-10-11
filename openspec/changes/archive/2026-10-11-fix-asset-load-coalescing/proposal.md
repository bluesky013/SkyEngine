## Why

Concurrent on-demand loads of the same unbuilt asset can intermittently **hang forever**. Reproduced with
`AssetManagerTest.ConcurrentLoadTest` (4 threads `LoadAsset(uuid)` then `BlockUntilLoaded()`): the hang is
non-deterministic and stops in `BlockUntilLoaded()` waiting on a wait handle that is never fulfilled.

Root cause (confirmed by instrumenting the load path + a baseline build without the recent thread-dispatcher
work, which reproduces identically — so this is **pre-existing**, not from the dispatcher change):

- `AssetManager::LoadAssetOnDemand` releases the coalescing guard (`pendingCooks`) **before** it establishes the
  wait handle (`asyncTask.second = promise->get_future()`) and registers the pending job.
- A late caller can therefore **win again** (the guard is already cleared once the first cook resolved and
  erased `pendingCooks`) and **overwrite** `asyncTask.second` with a new future.
- That late caller's `BuildRequest` is then **deduped away** (the shared `uuid#target` key is still held by the
  previous, just-finished cook task), so no cook ever fulfils the new promise.
- `BlockUntilLoaded` waits on the overwritten, unfulfilled future → deadlock.
- Additionally, `BlockUntilLoaded` reads `asyncTask.second` while `LoadAsset` may write it, an unsynchronized
  data race.

## What Changes

- Make on-demand coalescing **atomic**: under one lock, dedup per UUID, create the promise, set the wait handle,
  register the pending job, and schedule the cook. A concurrent caller for a UUID already in flight SHALL reuse
  the existing wait handle and MUST NOT overwrite it.
- Give the coalescing guard and the `uuid#target` build-dedup key consistent lifetimes so a scheduled cook is
  never silently dropped while a waiter depends on it.
- Fix the unsynchronized wait-handle read: read it once under the lock, then block **outside** the lock.
- Preserve coalescing (N concurrent loads → 1 cook) and keep the critical section O(1) with no new hot-path
  contention.

## Capabilities

### New Capabilities

<!-- none -->

### Modified Capabilities

- `asset-product-loading`: the "Concurrency safety for load and cook" requirement is tightened — on-demand
  coalescing SHALL be atomic so a concurrent load never orphans a wait handle.

## Impact

- `engine/framework/src/asset/AssetManager.cpp` (load/coalesce path), possibly `Asset.h`.
- `engine/test/framework/AssetManagerTest.cpp` (add a deterministic-then-stress regression test).
- No API/behavior change for callers; only the race is removed. No runtime behavior change otherwise.
