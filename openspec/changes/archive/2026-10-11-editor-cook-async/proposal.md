# Change: Thread-affine callback dispatch + non-blocking editor cooking

## Why

Triggering a cook from the editor (Save / Close / Assets menu / Import) currently risks freezing or
racing the UI. The cook *build* is already asynchronous (`AssetBuilderManager::RequestCook` →
`AssetExecutor::PushSavingTask`, a worker thread pool), so the build is not the blocker. The real
problems are:

1. **Callbacks run on the producing thread, not the consuming one.** `AssetBuilderManager::EmitBuildResult`
   runs inside the saving task and does `AsseEvent::BroadCast(OnAssetBuildFinished)` **on the worker**.
   The editor observer (`EditorAssetCatalog::OnAssetBuildFinished` → `OnBuildFinished` → cook state /
   `Refresh` / `MarkPaintDirty`) then mutates editor/UI state **off the main thread** — a data race
   and a hang/crash source (the freeze seen after Save→cook).
2. **Triggers happen inside event dispatch** (pointer/key) and are duplicated (viewer `SaveAsset` and
   `MaterialEditorView::Save` both cook; menu/import paths cook inline).
3. **Some paths block** (`CookWorker::CookBatch/CookAll` call `AssetExecutor::WaitForAll`).
4. **No pool isolation**: cooks share the loader pool despite a dedicated `SubmitCook`/`cookPool`.
5. **Save I/O is per-property**: `MaterialEditorView::Save` re-reads/parses/writes the whole source
   file once per property.

There is no general mechanism for "run this callback on thread X" today — every cross-thread hand-off
would otherwise hand-roll its own queue.

## What changes

- **Generic thread dispatch (`core/thread/ThreadDispatcher`).** A process-wide dispatcher with one
  queue **per `ThreadRole`** (`Main`, `Render`, `Device`, custom). Any thread `Post(role, callback)`
  (collect); the thread that owns a role calls `Dispatch(role)` in its loop to run that role's
  collected callbacks. `Application::Loop()` dispatches `ThreadRole::Main` once per frame; render /
  device loops dispatch their own roles. No role is special-cased in the mechanism.
- **Explicit cross-thread posts — no Event-system change.** The `Event`/`EventTraits` API stays as
  it is. At a known cross-thread site the producer posts explicitly, e.g.
  `AssetBuilderManager::EmitBuildResult` (worker) does
  `ThreadDispatcher::Get()->Post(ThreadRole::Main, [result] { AsseEvent::BroadCast(...); })` so the
  broadcast runs when the main role dispatches.
- **The completion callback (`onFinished`) is NOT broadcast state** and stays on the caller/worker
  thread — on-demand `LoadAsset(...)->BlockUntilLoaded()` runs on the main thread, so deferring that
  completion to the main queue would deadlock.
- **Editor: deferred, deduplicated cook requests.** `AssetCookService` becomes a queue
  (`RequestCook` / `RequestCookAll` insert, deduped by asset); `SandboxModule::Tick` drains it
  (main thread, once/frame). Event handlers only enqueue.
- **Pool isolation**: in-process cook builds run on the dedicated cook pool (`SubmitCook`); the
  out-of-process (`cookRunner`) branch and the D11 single-writer invariant are preserved.
- **Save I/O**: `MaterialEditorView::Save` reads the source once, mutates all dirty fields, writes
  once.
- **UI**: cook state (`BeginCook`) is kept; completion does a **local** refresh (no full shell
  rebuild).

## Impact

- Affected specs: new capability `editor-asset-cook`; `asset-async-executor` gains a
  "build completion is posted to the main role and broadcast there; load completion is not deferred"
  requirement.
- Affected code: `engine/core/thread` (`ThreadDispatcher`, `ThreadRole`),
  `engine/framework/application` (`Application::Loop` dispatches `Main`),
  `engine/framework/asset` (`AssetBuilderManager::EmitBuildResult` posts to `Main`; `AssetExecutor`,
  `CookWorker`), `engine/sandbox/core` (`AssetCookService`), `engine/sandbox/module`
  (`SandboxModule::Tick` + action handlers), `engine/sandbox/shell` (`AssetViewerWidget`),
  `engine/aurora/editor` (`MaterialEditorView`). **No `core/event` change.**
- Threading contract change: `IAssetEvent` callbacks now arrive on the main thread (because the emit
  site posts to `Main`) — audit all current listeners. No impact on deterministic lockstep (cook is
  editor/tooling).
