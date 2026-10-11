## Context

On-demand loads (`AssetManager::LoadAssetOnDemand`) coalesce concurrent requests for the same UUID so only one
cook runs. The coalescing is currently non-atomic and intermittently deadlocks `BlockUntilLoaded()`:

- `pendingCooks.insert(uuid)` is the guard, but it is released immediately; the wait handle
  (`loading->asyncTask.second = promise->get_future()`) and `pendingJobs[uuid]` are set **after** the guard is
  released, unsynchronized.
- Once the first cook resolves, `OnCookFinished` erases `pendingCooks[uuid]`, so a lagging caller can "win"
  again, **overwrite** `asyncTask.second`, and then have its `BuildRequest` **deduped away** (the `uuid#target`
  key is still held by the previous task's `savingTasks` entry) → the overwritten future is never fulfilled.
- `BlockUntilLoaded` also reads `asyncTask.second` concurrently with the writer (data race).

Evidence: instrumented run prints `[LOD] win`, then `[OCC] enter`, then a second `[LOD] win`, then
`[OCC] resolve`, then `[BUL] wait` and never `[BUL] done`. Baseline build without the recent dispatcher work
reproduces identically → **pre-existing**.

## Goals / Non-Goals

**Goals:**

- On-demand coalescing is **atomic**: a concurrent caller for an in-flight UUID reuses the existing wait handle
  and never overwrites it; exactly one cook is scheduled per UUID.
- No waiter is orphaned; `BlockUntilLoaded` unblocks on LOADED or FAILED (or a completed cook).
- No unsynchronized access to the wait handle.
- No performance regression: coalescing preserved, critical section O(1), blocking happens outside the lock.

**Non-Goals:**

- Change the cook pipeline, runners, or the completion event.
- Change the loader/cook pool split.
- Add new public API.

## Decisions

### Decision: Atomic coalescing under one lock

Under `mutex`: (1) find-or-create the asset, (2) if a load is already in flight for the UUID, return the
existing LOADING handle (reuse its wait handle), (3) else create the promise, set the wait handle, and register
`pendingJobs[uuid]`, and (4) only then release and schedule the cook.

Coalescing is keyed on the **presence of `pendingJobs[uuid]`** (the in-flight load), not on a separately-cleared
`pendingCooks` set — so the guard and the wait handle have the same lifetime. Before starting a load, re-check
the asset's status under the lock and return early if it is already LOADED (or FAILED) so a post-completion
caller never starts a redundant second cook.

### Decision: Wait handle is set once; waiters reuse it

The wait handle SHALL be written exactly once per load and never overwritten while the load is in flight. The
`uuid#target` build-dedup key and the in-flight load therefore cannot disagree: a scheduled cook always has a
live waiter, and a waiter always has a cook.

Alternatives: keeping a separate `pendingCooks` guard (rejected — its early erase is the bug); erasing the build
key from the waiter side (rejected — couples the loader to the executor's dedup internals).

### Decision: Wait outside the lock (shared_future)

`BlockUntilLoaded` reads the wait handle under the lock, copies it, releases the lock, and waits **outside** it.
Because `std::future` is move-only, the handle is stored as `std::shared_future<void>` (copyable, O(1) to copy).

This is required for correctness: `OnCookFinished` takes the same lock to resolve, so waiting while holding it
would deadlock.

### Performance

- **Coalescing is the main performance property and it is preserved**: N concurrent loads of one UUID → exactly
  one cook; the N waiters share one `shared_future` (atomic refcount inc, no per-waiter allocation).
- **Critical section is O(1)**: a map/set lookup + insert + `std::promise` creation. No file I/O, no cook, and
  no blocking wait happen under the lock, so loaders do not serialize on each other beyond the bookkeeping.
- **Scheduling the cook happens after the lock is released** (PushCookTask only enqueues), so the shared
  `mutex` is not extended by executor work.
- **No polling/spinning**: completion is event-driven (`OnCookFinished` → `set_value`).
- **No extra hot-path allocation**: existing `pendingJobs`/`pendingCooks` containers are reused; the only heap
  object is the `shared_ptr<std::promise>` already used today.

## Risks / Trade-offs

- [Holding `mutex` across find-or-create + promise creation slightly widens the critical section] -> Bounded,
  O(1); the previous code already held the lock for the insert, and no I/O/cook is inside.
- [Changing `std::future` to `std::shared_future` touches `AssetBase::asyncTask`] -> Mechanical; the wait
  semantics are the same, and it removes the data race.
- [Re-checking loaded status under the lock adds a branch] -> Negligible; prevents redundant cooks.

## Migration Plan

Behavioral no-op for callers. Verified by: the existing `ConcurrentLoadTest` plus a new stress regression
(many threads × many iterations) that must not hang, run in both build configs.

## Open Questions

- None blocking.
