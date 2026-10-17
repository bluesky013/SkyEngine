## Why

`AssetManagerTest.ConcurrentLoadStress` intermittently failed when 8 threads loaded the same product UUID: some `LoadAsset` calls returned null with "Asset handler not registered". The file-based load path had no coalescing: every caller re-opened the product, re-read its header and raced the asset's dependency/status state. The on-demand path already coalesces through `pendingJobs`; the file path did not.

## What Changes

- Serialize file-based `AssetManager::LoadAsset` orchestration with a recursive `loadMutex`.
- Treat an asset already in `LOADING` as an in-flight result and return it so callers wait on its existing handle instead of re-reading the product header.
- Keep recursive dependency loading working (the mutex is recursive).
- Add the concurrent-load requirement to the `asset-product-loading` spec.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `asset-product-loading`: concurrent loads of the same product UUID coalesce to one header read/in-flight load and return the same waitable asset.

## Impact

- `engine/framework/include/framework/asset/AssetManager.h`
- `engine/framework/src/asset/AssetManager.cpp`
- Fixes the intermittent `FrameworkTest.ConcurrentLoadStress` failure; no public API change.
- The lock is process-local and only covers load orchestration; asset map access keeps its existing mutex.
