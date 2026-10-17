## 1. Implement coalescing

- [x] 1.1 Add a recursive `loadMutex` to `AssetManager`.
- [x] 1.2 Lock file-based `LoadAsset` orchestration and return `LOADING`/`LOADED` assets directly.

## 2. Verify

- [x] 2.1 Rebuild `FrameworkTest`.
- [x] 2.2 Run `FrameworkTest` / `ctest -R FrameworkTest` repeatedly; confirm `ConcurrentLoadStress` no longer fails.
- [x] 2.3 Run the full test suite and validate the change.

## Verification record

- `FrameworkTest` and `ctest -R FrameworkTest` passed three consecutive runs after the change (previously failed intermittently at `AssetManagerTest.cpp:991`).
- Full `ctest` passed 49/49.
- `openspec validate fix-asset-load-coalescing --strict --no-interactive` and `git diff --check` passed.
