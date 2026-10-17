## Context

`AssetManager::LoadAsset(uuid)` has two paths:

- file/product path: `FindAsset` → `OpenFile` → `CreateAssetByHeader` → schedule async load;
- on-demand path: `LoadAssetOnDemand` with `pendingJobs` coalescing.

The on-demand path already coalesces. The file path did not: each concurrent caller could read the same product header and race dependency/status writes. The stress test occasionally reported "Asset handler not registered" and null results.

## Goals / Non-Goals

**Goals:**

- Make concurrent file loads of the same UUID safe and coalesced.
- Preserve recursive dependency loading.
- Keep the change local to `AssetManager`.

**Non-Goals:**

- No per-asset parallelism redesign.
- No change to on-demand cook coalescing.
- No public API change.

## Decisions

### 1. Recursive load-orchestration mutex

Add `std::recursive_mutex loadMutex` to `AssetManager` and hold it across the file-path load orchestration. This makes the header read and the `LOADING` transition atomic with respect to other `LoadAsset` calls, and the recursive type allows dependencies to load from the same thread.

### 2. Return in-flight assets

After `FindAsset`, return immediately when the asset is `LOADED` or `LOADING`. The second caller then waits on the handle established by the first caller instead of opening the product again.

### 3. Keep the existing asset-map mutex separate

`mutex` still protects the asset weak map; `loadMutex` only serializes orchestration. This keeps map lookups independent and makes the intent clear.

## Risks / Trade-offs

- **[Global serialization of loads]** → Product header reads are small and the on-demand path already coalesces; correctness is more important than parallel header I/O.
- **[Holding a lock across async scheduling]** → The lock is released after `SetAsyncTask`; the async work itself does not acquire `loadMutex`.
- **[Dependency recursion]** → Recursive mutex is required and tested by dependency-bearing assets.

## Migration Plan

Single code change plus repeated stress runs. Rollback is a straight revert.

## Open Questions

None.
