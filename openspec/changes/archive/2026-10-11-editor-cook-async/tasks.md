# Tasks

> Implemented (see proposal.md, design.md). Verified: sandbox-OFF + sandbox-ON builds; new ThreadDispatcher /
> catalog-coalescing tests pass.

## Core: thread dispatch

- [x] 1. Add `ThreadDispatcher` (`core/thread/ThreadDispatcher.h`, cross-DLL `Singleton`): a
      `ThreadRole` enum (`Main`, `Render`, `Device`, extendable) with one queue per role;
      `Post(role, callback)` (any thread) + `Dispatch(role)` (owner thread) + a `PostToMain(cb)`
      convenience. Per-role mutex; `Dispatch` swaps the queue to a local batch under the lock and
      runs callbacks **outside** the lock (reentrant-safe, no slow work under lock).
- [x] 2. `Application::Loop()` dispatches `ThreadRole::Main` at the start of each frame (before
      `PreTick` / `moduleManager->Tick`). Shutdown: **join worker threads first** (`AssetExecutor`
      pools, OOP monitor), **then** clear all role queues.

## Framework: explicit cross-thread posts (no Event change)

- [x] 3. Post every off-main-thread `AsseEvent` broadcast to `Main`: `AssetBuilderManager::EmitBuildResult`
      (worker), `OutOfProcessCookRunner::NotifyCompletion` (OOP monitor thread), and
      `AssetManager::LoadInto` (`OnAssetLoaded`). Keep `onFinished` on the caller/worker thread (load
      waiters block the main thread). Do **not** add a delivery trait to `Event`/`EventTraits`.
- [x] 4. Audit all `IAssetEvent` listeners for the main-thread contract; add a debug thread-id assert
      on `Main`.

## Framework: cook pool without losing dedup / WaitForAll

- [x] 5. `AssetExecutor`: unify task tracking — `PushTask(key, pool, cb)` with dedup on `key`;
      `PushSavingTask` (loader pool) / `PushCookTask` (cook pool); `WaitForAll` covers both pools.
      `AssetBuilderManager::BuildRequest` switches to `PushCookTask` with the same `uuid#target` key.
      Fold or remove the bare `SubmitCook`.

## Editor

- [x] 6. `AssetCookService` → request queue: `RequestCook(uuid)` / `RequestCookAll()` only insert
      (dedup by asset), add `Drain()` (BeginCook + BuildAllTargets per request).
- [x] 7. `SandboxModule::Tick`: in the editor branch (`!hubMode`), `AssetCookService::Drain()` before
      `renderer.Tick`, and `EditorAssetCatalog::FlushRefresh()` once per frame.
- [x] 8. `EditorAssetCatalog`: `OnBuildFinished` updates per-asset cook state and sets
      `refreshPending` (cheap, no scan); `FlushRefresh()` does the mount rescan once; `Refresh()`
      stays read-only (no `BroadCast` / `Post` / cook — no re-entrancy).
- [x] 9. Update callers to enqueue only: `SandboxModule` (`asset.cook` / `asset.cookAll` / import),
      `AssetViewerWidget::SaveAsset`, `MaterialEditorView::Save` (drop the duplicate cook).
- [x] 10. UI: keep `BeginCook` state; local refresh only (no full shell rebuild); non-blocking
      "cooking…" indicator.
- [x] 11. `MaterialEditorView::Save`: read the source once, apply all dirty-field mutations, write
      once (replace the per-property read/parse/write in `PersistProperty`).

## Tests / docs

- [x] 12. Tests: a worker `Post(ThreadRole::Main, …)` runs on the main thread after `Dispatch(Main)`
      (thread-id assert); `RequestCook` does not build synchronously; N requests before a drain → 1
      build; N completions in one frame → **one** catalog refresh; on-demand `LoadAsset` +
      `BlockUntilLoaded` still returns (no deadlock regression).
- [x] 13. Docs: update `asset-async-executor` notes and `skyengine-coding-rules` rule 9 to point at
      the deferred queue / target-thread dispatch.
