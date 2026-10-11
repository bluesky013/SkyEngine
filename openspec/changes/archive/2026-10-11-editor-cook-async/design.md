# Design: Non-blocking, main-thread-safe editor cooking

## Context

Cook flow today:

```
UI event -> AssetCookService::TriggerCook
  -> EditorAssetCatalog::BeginCook                 (main: sets cook state)
  -> AssetDataBase::BuildAllTargets
       -> AssetBuilderManager::RequestCook -> BuildRequest
            -> AssetExecutor::PushSavingTask(lambda)     // async, loader pool
                 [worker] builder->Request(request, result)
                 [worker] EmitBuildResult(result, onFinished)
                            -> AsseEvent::BroadCast(OnAssetBuildFinished)   <-- worker thread!
                            -> onFinished(result)                           <-- worker thread
```

Findings (verified):
- The build is already on a worker; `TriggerCook` does not wait for it.
- `EmitBuildResult` (`AssetBuilderManager.cpp:209`) runs **on the worker** and broadcasts
  `OnAssetBuildFinished` there. The editor listener (`EditorAssetCatalog.cpp:108`) then mutates
  cook state / `Refresh` / `MarkPaintDirty` **off the main thread** — the freeze/race.
- No main-thread post mechanism exists (grep: none).
- `Application::Loop()` (`Application.cpp:141`) is `PreTick → tickFn → moduleManager->Tick`.
- **Main-thread blocking loads exist**: `AuroraAssetViews.cpp` / `MaterialEditorView.cpp:821` call
  `LoadAsset(...)->BlockUntilLoaded()` on the main thread. So load-completion must NOT be deferred.
- `SandboxModule::Tick` (`:1196`) is the editor's per-frame main-thread hook.

## Decision

Delivery is **explicit at the cross-thread call sites**, through a generic per-thread-role
dispatcher; the editor additionally defers/coalesces cook triggers.

### 1. `ThreadDispatcher` — one queue per thread role (generic)

The mechanism: **a process-wide dispatcher holds one queue per `ThreadRole`; any thread collects a
callback into a role's queue; the thread that owns that role drains it in its loop.** No role is
special-cased — `Main` is just the role the application loop happens to own.

`core/thread/ThreadDispatcher.h` (cross-DLL `Singleton`; per-role queue, double-buffered swap):

```cpp
enum class ThreadRole : uint32_t { Main = 0, Render, Device, Count };  // extendable

using Callback = std::function<void()>;

class ThreadDispatcher : public Singleton<ThreadDispatcher> {
    friend class Singleton<ThreadDispatcher>;
public:
    void Post(ThreadRole role, Callback cb);  // any thread; short critical section (push only)
    void Dispatch(ThreadRole role) {          // owner thread only
        Queue &q = queues[static_cast<size_t>(role)];
        std::vector<Callback> batch;
        {
            std::lock_guard<std::mutex> lock(q.mutex);
            batch.swap(q.pending);            // O(1) swap inside the lock
        }
        for (auto &cb : batch) { cb(); }      // run OUTSIDE the lock
    }
private:
    struct Queue { std::mutex mutex; std::vector<Callback> pending; };
    std::array<Queue, static_cast<size_t>(ThreadRole::Count)> queues;
};
```

Locking rules (both for correctness and cost):
- **Never run callbacks under the lock.** `Post` only `push_back`s; `Dispatch` only `swap`s to a
  local batch (O(1)) then runs outside the lock. A callback that calls `Post` (reentrant) is safe;
  a slow callback does not block other producers.
- **One mutex per role**, not one global mutex — `Main` / `Render` / `Device` do not contend.
- Reentrant posts during `Dispatch` land in the now-empty `pending` and are handled next dispatch
  (no self-recursion).
- If it ever becomes hot, a lock-free MPSC stack (`std::atomic<Node*>` head, `exchange(nullptr)` in
  `Dispatch`) is the next step; not needed for once-per-frame dispatch.

Each role's owner loop dispatches it: `Application::Loop()` calls
`ThreadDispatcher::Get()->Dispatch(ThreadRole::Main)` at the **start of the frame** (before `PreTick`
/ `moduleManager->Tick`); a render or device loop calls `Dispatch(ThreadRole::Render)` /
`Dispatch(ThreadRole::Device)` on its own iteration. Callbacks posted from any thread to another
role's queue are safe to mutate that role's state when dispatched.

Why one dispatcher with per-role queues rather than a queue per event type: a per-type queue would
force `Application` to keep a registry of every event's queue. Roles are the natural granularity for
thread affinity, and the `Post(role, …)` API also serves non-event cross-thread callbacks (e.g.
out-of-process cook results). Roles start as a small fixed set (`Main`, `Render`, `Device`); custom
roles can be added later without changing the mechanism.

### 2. Cross-thread posts are explicit at the call site (no Event change)

The `Event` / `EventTraits` API is **not** changed — no target-thread trait. At a known cross-thread
site the producer posts explicitly to the dispatcher. For asset build results:

```cpp
// AssetBuilderManager::EmitBuildResult, running on the worker:
ThreadDispatcher::Get()->Post(ThreadRole::Main, [result]() {
    AsseEvent::BroadCast(result.uuid, &IAssetEvent::OnAssetBuildFinished, result); // now on main
});
if (onFinished) { onFinished(result); }  // stays on the worker (see §3)
```

So the broadcast runs when the main role dispatches. Deferral requires the broadcast args to be
copyable (`AssetBuildResult` is: `Uuid` + `std::string` + enum).

The **same treatment applies to every `AsseEvent` broadcast that can run off the main thread**, so
the contract is uniform ("all `AsseEvent` callbacks run on `Main`"):

```cpp
// AssetManager::LoadInto (may run on the load worker or the main thread):
ThreadDispatcher::Get()->Post(ThreadRole::Main, [uuid = asset->uuid]() {
    AsseEvent::BroadCast(uuid, &IAssetEvent::OnAssetLoaded);
});
```

Sites to convert: `AssetBuilderManager::EmitBuildResult` (worker),
`OutOfProcessCookRunner::NotifyCompletion` (OOP monitor thread),
`AssetManager::LoadInto` (`OnAssetLoaded`). `OnAssetReload` / `OnAssetReset` are not broadcast today;
if added, they follow the same rule. Add a `ThreadDispatcher::PostToMain(cb)` convenience (thin
`Post(ThreadRole::Main, …)`) to avoid repeating the role at every site.

Why not a per-event delivery trait: it would make every broadcast of that event
(`OnAssetLoaded` / `OnAssetReload` / `OnAssetReset` too) change thread, and force a trait on the
generic `Event` for one site's need. An explicit `Post` at the few sites that actually cross threads
is smaller and local; the dispatcher stays the only shared mechanism.

### 3. The completion callback is not broadcast state → stays direct

`AssetBuilderManager::EmitBuildResult` also calls `onFinished(result)`. That callback resolves
on-demand `LoadAsset` waiters, and some waiters block the main thread
(`LoadAsset(...)->BlockUntilLoaded()` in `AuroraAssetViews.cpp` / `MaterialEditorView.cpp:821`).
It therefore **stays on the caller/worker thread**; deferring it onto the main queue would deadlock
(the main thread waits inside `BlockUntilLoaded` and can never dispatch the queue). Callers that need a
main-thread `onFinished` (none today) must post it themselves.

### 4. Editor: deferred, deduplicated cook requests + coalesced refresh

`AssetCookService` becomes a request queue:

```cpp
void RequestCook(const Uuid &uuid);   // insert into a set<Uuid> (dedup per asset); event-safe
void RequestCookAll();                // enqueue all sources (coalesced)
void Drain();                         // main thread: for each → BeginCook + BuildAllTargets
```

**Drain site**: `SandboxModule::Tick`, in the editor branch (`!hubMode`), **before** `renderer.Tick`.
Frame order is therefore: `Application::Loop` dispatches `Main` (cook completions) → `Tick` drains new
requests → render. Requests are editor-only; hub mode has no project.

**Coalesced refresh**: one frame can complete many cooks → many `OnAssetBuildFinished`. Running
`EditorAssetCatalog::Refresh()` (which rescans mounts) per completion would hitch the main thread.
Instead `OnBuildFinished` only updates per-asset cook state (cheap) and sets `refreshPending`;
`SandboxModule::Tick` then calls `EditorAssetCatalog::FlushRefresh()` **once per frame**, which does
the rescan once. `Refresh()` stays read-only (rebuilds catalog entries; must not
`BroadCast` / `Post` / cook), so it cannot re-enter.

(Per-target dedup stays in the framework `PushTask` key `uuid#target`; the editor set dedups per
asset, enough because `BuildAllTargets` re-derives targets.)

### 5. Pool isolation (cook vs loader) — without losing dedup / `WaitForAll`

Today `AssetExecutor::PushSavingTask(key, func)` dedups by `key` (`uuid#target`) in its task list and
is tracked by `WaitForAll()`, and `BuildRequest` uses it for **all** build work (cook, including
on-demand load builds) — so cook currently runs on the **loader pool**. Moving cook off the loader
pool must keep both dedup and `WaitForAll`.

Design: keep the single tracked/deduped task list; make the pool a parameter.

```cpp
void PushTask(const std::string &key, ThreadPool &pool, Callback cb); // dedup on `key`, tracked
void PushSavingTask(key, cb) { PushTask(key, pool,     std::move(cb)); } // non-cook save → loader
void PushCookTask  (key, cb) { PushTask(key, cookPool, std::move(cb)); } // cook → cook pool
void WaitForAll();  // waits every tracked task (both pools)
```

`AssetBuilderManager::BuildRequest` switches to `PushCookTask` with the **same** `key = uuid#target`,
so the dedup semantics are unchanged. The bare `AssetExecutor::SubmitCook` (a `cookPool.Schedule` with
no dedup/tracking) is folded into `PushCookTask` (or removed if it has no other user).

- The out-of-process (`cookRunner`) result path is a **second** broadcast site
  (`OutOfProcessCookRunner::NotifyCompletion`, on the OOP monitor thread); it must post its
  `OnAssetBuildFinished` broadcast to `ThreadRole::Main` the same way.
- Keep the D11 single-writer invariant (when out-of-process is active the editor does not cook).

### 6. `MaterialEditorView::Save` single read/write

Read the source once into a `rapidjson::Document`, apply every dirty field's mutation to that
document, then serialize + write once (replaces the per-property read → parse → write loop in
`PersistProperty`). Keep the `dirty` short-circuit.

### 7. UI

`BeginCook` already marks state; on main-thread completion do a **local** refresh and clear state —
no full shell rebuild. A non-blocking "cooking…" indicator is enough.

### 8. Cost notes

- **One heap allocation per posted callback.** `std::function<void()>` capturing `AssetBuildResult`
  (contains a `std::string`) exceeds the small-buffer limit → one allocation per completed cook.
  Acceptable (cook completions are infrequent). If it ever matters, swap `Callback` to a
  small-buffer move-only callable (or capture a `shared_ptr<const AssetBuildResult>`); not needed now.
- **Batch, don't loop per item.** `Dispatch` runs the whole swapped batch in one call; the coalesced
  catalog refresh (§4) keeps per-frame main-thread work bounded regardless of batch size.

## Shutdown

`ThreadDispatcher` is a cross-DLL `Singleton` in `Environment`. Teardown order matters:

1. **Join worker threads first** (`AssetExecutor` pools, the out-of-process monitor) so no thread can
   `Post` after this point. Draining before joining still races with in-flight workers.
2. **Then clear all role queues** (`Clear()` / dispatch-and-drop) so no posted callback runs against
   half-destroyed state.

## Alternatives considered

- **Editor-only queue** (worker → editor queue → main consume): smaller, but leaves the
  `AsseEvent::OnAssetBuildFinished` broadcast worker-threaded for the framework/other observers.
  Rejected (decided: a generic core `ThreadDispatcher` + explicit posts).
- **Per-event delivery trait on `Event`**: forces every broadcast of that event to change thread and
  adds a trait to the generic `Event` for one site's need. Rejected (see §2).
- **Block on the UI thread (`WaitForAll`)**: freezes by construction. Rejected.
- **Marshal `onFinished` too**: deadlocks main-thread `BlockUntilLoaded`. Rejected (see §3).

## Behavior changes / scope notes

- **Ordering change**: today the worker broadcasts `OnAssetBuildFinished` *then* calls `onFinished`,
  both synchronously. After the change the broadcast is deferred to the next main dispatch, so a
  listener may now observe `onFinished` (which stays immediate) *before* the broadcast. Audited: the
  two are independent (cook state vs on-demand load), but keep this in mind for new listeners.
- **Failure paths**: `EmitBuildResult`'s early `asset not found` / `source missing` / `no builder`
  cases run on the caller thread (often main); they post to `Main` too, so their broadcast is
  deferred one frame. Harmless, but it means "broadcast happened by the time `BuildRequest` returns"
  no longer holds anywhere.
- **Only `Main` is used today**; `Render` / `Device` are placeholders for the generic mechanism
  (adding a role = adding an enum value; the mechanism is unchanged).
- **`OnAssetLoaded` is also broadcast off the main thread** (`AssetManager::LoadInto`, from the load
  worker). It has no `AsseEvent` listener today, but a future listener would hit the same race.
  Out of scope here (see tasks) — decide later whether to post it too or route loads differently.

## Verification

- Headless: `ThreadDispatcher::Post(ThreadRole::Main, …)` from a worker + `Dispatch(ThreadRole::Main)`
  runs the callback on the main thread (thread-id assert). `EmitBuildResult` for a cooked asset →
  observer runs on the main thread.
  On-demand `LoadAsset` + `BlockUntilLoaded` still returns (no regression).
- Dedup: N repeated `RequestCook` before a `Drain` → one build.
- `RequestCook` returns without running a build (no synchronous work in the trigger).
- Manual: edit a material property, `Save` / `Close` / `Cook` → UI stays responsive; repeated Save
  does not spawn duplicate cooks.
