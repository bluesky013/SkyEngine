## Context

`asset-pipeline` defines an out-of-process cook mode but deferred its implementation. The engine today can only run a cook in-process: `AssetBuilderManager::BuildRequest` pushes an `AssetExecutor::PushSavingTask` that runs the builder and broadcasts `IAssetEvent::OnAssetBuildFinished` (`engine/framework/src/asset/AssetBuilderManager.cpp:124-138`); `CookWorker` drains a work list over that same path and calls `AssetExecutor::WaitForAll()` (`engine/framework/src/asset/CookWorker.cpp:13-33`). A long or crashing cook therefore lives inside the editor process.

There is no lower-level facility to build on. The only process call is `Platform::RunCmd` (`engine/framework/include/framework/platform/PlatformBase.h:40,98`), implemented only on Windows via a blocking, read-only `_popen` (`engine/framework/platform/windows/Win32Platform.cpp:153-165`). There is no child-process spawn, no pipe, no stdin write, and no request/response correlation.

Constraints from the repo:
- `AGENTS.md` module layering: engine modules hold interfaces and interface data; the thin platform primitive may live in the existing `engine/framework/platform` next to `RunCmd`.
- The engine avoids heavy, engine-wide runtime dependencies (it already replaced taskflow in the asset path with its own `sky::ThreadPool`); no IPC library is vendored (no Boost.Asio/Interprocess/Process — the Boost bootstrap whitelist excludes them — and no ZeroMQ/nng/gRPC).
- Out-of-process cook is a **builder/editor-side** concern. The runtime has no source catalog and no cook (`asset-pipeline` D3), and mobile forbids arbitrary child processes, so mobile must be explicitly excluded.
- Depends on `asset-pipeline` task 10.1, already implemented: `AssetBuildResult` carries `uuid`/`target`/`retCode`/`error` so completions can be correlated.

## Goals / Non-Goals

**Goals:**
- A minimal child-process + stdio-pipe primitive for desktop: **Windows** (primary), **macOS** (primary) via a shared POSIX backend, and **Linux** (lower priority) via the same POSIX backend plus a minimal Linux build backend (D9).
- A defined length-prefixed frame codec so stdout carries protocol frames and stderr carries logs.
- An `ICookRunner` seam where in-process and out-of-process cook raise the **same** completion event, so the loading layer is mode-agnostic (`asset-pipeline` D7).
- An `AssetTool` worker host that reuses `CookWorker` and starts with the editor's mount namespace and platform target.
- Robust lifecycle: response correlation, per-request timeout, crash/timeout → fail the request (never strand a `LOADING` asset) and restart on demand.
- Zero new third-party runtime dependency.

**Non-Goals:**
- Changing identity, manifests, product layout, or the loading state machine (owned by `asset-pipeline`).
- A general-purpose IPC/RPC framework, streaming, or multiplexing beyond request/response.
- Out-of-process cook on Android/iOS.
- Incremental/freshness build, packaging, or full taskflow removal.
- Secure sandboxing of the worker (it is a trusted, first-party tool).

## Decisions

### D1. Build vs buy: self-developed thin layer

**Decision:** implement a small process/pipe primitive and the frame protocol in-tree; add **no** IPC third-party dependency.

Alternatives considered:
- **libuv** (mature cross-platform async process + pipe, powers Node): strongest "buy" candidate, but it is a heavyweight event-loop runtime, would duplicate the engine's own thread/IO model, and requires a new bootstrap entry plus per-platform build and bundling. Overkill for one blocking request/response worker.
- **Boost.Process / Boost.Asio**: Boost is already bootstrapped header-only, but its whitelist (`cmake/thirdparty.json`) excludes `libs/process`, `libs/asio`, and `libs/interprocess`; enabling them re-bootstraps Boost and pulls in `boost::system`/platform backends. `boost::process` is also not header-only on all toolchains.
- **nng / ZeroMQ / nanomsg**: mature message transports, but they solve brokered sockets, not child-process stdio. They add a native dependency and a wire format when the worker is a local pipe child.
- **gRPC / Cap'n Proto / FlatBuffers / protobuf**: far too heavy for four fields; would add codegen and a schema toolchain.
- **`subprocess.h` single-header**: minimal, but read-only and Windows-light; it would only cover spawn and still leave framing, lifecycle, and timeout to us, so the net saving is small.

Rationale: the protocol is tiny (one request, one result) and local; the engine's philosophy favors owning small, testable primitives. The reusable surface is roughly a few hundred lines per OS. Maturity-risk is contained because the wire protocol and lifecycle are ours and unit-tested.

### D2. Process primitive and platform scope

Add `Process` to the framework platform layer (`engine/framework/platform/`, alongside `RunCmd`), with a backend per OS:

```
class Process {
    bool Start(const ProcessDesc &desc);   // argv, env, cwd
    bool WriteStdin(const uint8_t *data, size_t size);
    size_t ReadStdout(uint8_t *dst, size_t cap, bool &eof);
    size_t ReadStderr(uint8_t *dst, size_t cap, bool &eof);
    bool WaitFor(uint32_t timeoutMs, int &exitCode);  // false on timeout
    void Kill();
    bool IsRunning() const;
};
```

- **Windows**: `CreateProcessW` with `bInheritHandles`, `CreatePipe` (or `CreateNamedPipe` for overlapped IO), stdin/stdout/stderr redirected; `WaitForSingleObject` + `TerminateProcess`; a Job Object with `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE` for kill-on-parent-exit.
- **macOS + Linux (one shared `platform/posix` backend)**: `posix_spawn` with `posix_spawn_file_actions_adddup2`/`addclose`; `pipe()` + `fcntl(F_SETFD, FD_CLOEXEC)` (NOT `pipe2`, which is Linux-only); working directory via `posix_spawn_file_actions_addchdir_np` (macOS 10.15+ / glibc 2.29+) guarded with a `fork`+`execve` fallback; child process group via `POSIX_SPAWN_SETPGROUP` + `setpgroup(0)` so `kill(-pgid)` tears down the whole tree; ignore `SIGPIPE` process-wide; `waitpid(WNOHANG)` polling for the timeout; `kill(SIGKILL)`.
- **Android/iOS**: `Start` returns `false` with an "unsupported" status; callers fail fast.
- stdout/stderr are read on dedicated reader threads (below), so `Read*` may block up to a short poll interval.

Portable POSIX subset actually shared: `posix_spawn`/`posix_spawn_file_actions_*`, `posix_spawnattr_*`, `pipe`+`fcntl`, `waitpid(WNOHANG)`, `kill`. **Not** portable and therefore avoided in the shared path: `pipe2`, `prctl`/parent-death signals, `addchdir_np` without a guard, `SO_NOSIGPIPE` (macOS) / `MSG_NOSIGNAL` (Linux). Linux is a lower-priority target that additionally needs the build backend in D9 before it can compile/run; a Linux-only `prctl(PR_SET_PDEATHSIG)` may be added later behind `#ifdef __linux__` but is not required (explicit `Drain`/`Kill` is the guarantee).

**`fork`+`execve` fallback safety (required when `addchdir_np` is unavailable):** after `fork`, between the child branch and `execve` only **async-signal-safe** operations are permitted (POSIX). The fallback therefore MUST: pre-build the `argv`/`envp` arrays and the absolute executable path **before** forking (no allocation in the child), call only `dup2`/`close`/`chdir`/`execve` and `_exit` on failure, use no `malloc`, no locks, no logging, and no C++ objects; `errno` is captured by encoding the failure into the child's exit code. Prefer `posix_spawn` with `addchdir_np` whenever the platform provides it, and treat the fallback as a narrow, well-tested path; do not run it from an arbitrary heavily-threaded context if avoidable.

`Platform::RunCmd` MAY be reimplemented on top of `Process` to remove the `_popen` shell dependency, but that is optional and not required by this change.

### D3. Transport: stdout frames, stderr logs

- One worker ↔ one pipe pair. **stdout** carries length-prefixed frames only; **stderr** carries human-readable logs and is pumped into the engine logger (tag `CookWorker`) so it can never corrupt the stream (`asset-pipeline` D14).
- **Logger collision (verified hazard, root fix in place)**: `LOG_*` wrote to stdout via a hardcoded `printf` (`engine/core/src/logger/Logger.cpp:29`) and `Logger::SetOutputCallback` is *additive*, so the worker WOULD corrupt frames on every log. **Root fix (landed)**: `Logger` now has `Logger::SetOutputStream(FILE*)` (default stdout, unchanged behavior); the worker calls `Logger::SetOutputStream(stderr)` **before any engine initialization**. This removes the collision at the source rather than masking it.
- **Defense in depth**: because stray non-`Logger` stdout writers exist in the engine (e.g. `engine/render/backend/vulkan/src/Swapchain.cpp:224`, `engine/framework/src/application/ToolApplicationBase.cpp:31`), the worker ALSO `dup`s the real stdout to a private fd used only for frames and `dup2`s stderr onto fd 1 (Windows `_dup`/`_dup2`; POSIX `dup`/`dup2`). Belt-and-suspenders: frames stay isolated even if some dependency writes to stdout directly. This is a required worker-boot step.
- **Frame encoding**: `[u32 little-endian payloadLength][payloadBytes]`, payload is UTF-8 JSON. A `maxFrameBytes` cap (default 16 MiB) rejects desynchronized/oversized frames and fails the connection instead of allocating unboundedly.
- Partial frames: the reader accumulates into a growable buffer with read/write loops; a short read never loses bytes; on cancellation/EOF the pipe is drained and the channel closed.
- Rationale: length-prefixing is trivial, language-agnostic, and binary-safe; JSON keeps messages human-inspectable and matches the repo's rapidjson usage (`3rdParty::rapidjson`, already linked by `Core`).

### D4. Message schema and correlation

Messages are JSON objects with a `type` discriminator:

- Handshake: request `{"type":"hello","protocol":1}` → response `{"type":"ready","protocol":1,"platform":"<target>"}`; a protocol mismatch fails the session.
- Cook: `{"type":"cook","id":<u64>,"uuid":"<uuid>","target":"<target>","path":"<logical path>"}` → `{"type":"result","id":<u64>,"uuid":"<uuid>","target":"<target>","retCode":<int>,"error":"<string>"}`.
- Control: `{"type":"ping"}`/`{"type":"pong"}` for liveness; `{"type":"shutdown"}` for clean teardown.

Correlation: the monotonic `id` matches a result to its in-flight request at the transport layer; `uuid`+`target` are carried through so the completion event and `asset-pipeline`'s per-UUID coalescing stay correct. Unknown/duplicate `id`s are logged and dropped.

### D5. `ICookRunner` seam and mode-agnostic completion

Add to `engine/framework/asset`:

```
class ICookRunner {
    virtual ~ICookRunner() = default;
    virtual bool Request(const CookJob &job) = 0;  // {uuid, target, path}
    virtual void Drain() = 0;
};
```

- `InProcessCookRunner` wraps the existing `CookWorker` on the **cook pool** (off the loader pool, `asset-pipeline` D14) and broadcasts the completion event directly.
- `OutOfProcessCookRunner` owns a `Process` + `FrameChannel`, sends `cook` frames, and on each `result` frame builds an `AssetBuildResult` (`uuid`/`target`/`retCode`/`error`) and raises the **same** `IAssetEvent::OnAssetBuildFinished`.
- The loading layer depends only on `ICookRunner` + `IAssetEvent`, so in-process and out-of-process are indistinguishable to it (this replaces the current in-process `BuildRequestSync` shortcut noted in `asset-pipeline` D7).
- Mode is selected by cook config (`configs/asset_cook.jsonc`: `cook.mode = "in-process" | "out-of-process"`, plus `cook.worker.path`), defaulting to `in-process`.

### D6. Worker host (`AssetTool`) and same-namespace startup

- New executable `tools/asset_tool` (a builder-side tool, **built independent of `SKY_BUILD_TOOL`**; `tools/CMakeLists.txt` is currently gated and `asset_builder` is commented out — this change adds the wiring), linking `Framework`.
- On `start`, it reads its mount namespace and platform target from argv/env passed by the editor (e.g. `--mount <root>:<ro|rw>` repeated, `--platform <target>`), builds the same `MultiFileSystem`, and replies `ready`. It reuses `CookWorker` to drain `cook` jobs.
- It sets up its own `AssetManager`/product writing exactly as the editor does, so products and `product.index` updates are identical.
- Logs go to stderr; only frames go to stdout (via the fd redirection above).
- **Executable discovery**: the editor resolves `cook.worker.path` (default: a sibling `AssetTool[.exe]` next to the editor, derived from `Platform::GetBundlePath()`), and the build/deploy places it there (`output/bin/<config>`). There is no existing sibling-exe locator, so this is new code.

### D7. Lifecycle, timeout, and restart

- One **persistent** worker per session; requests are queued and matched by `id`.
- A dedicated **reader thread** drains stdout (frames) and another pumps stderr (logs). A writer mutex serializes `cook` frames.
- **Per-request timeout** (configurable, default e.g. 10 min). On timeout or unexpected EOF/exit: mark the worker dead, **fail all in-flight requests** (each raises `OnAssetBuildFinished` with `retCode != 0` so the pending load becomes `FAILED`, never stranded), `Kill()` the process, and allow a **fresh worker to be spawned on the next request**.
- `Drain()` shuts the worker down (`shutdown` frame, then `Kill()` if it does not exit) so no process outlives the editor.
- **Reader-thread shutdown (required)**: the stdout/stderr reader threads block in `ReadFile`/`read`, so a plain flag cannot interrupt them. Shutdown MUST follow a fixed order: (1) signal stop and close the **write** end of the child's stdin so the child sees EOF; (2) unblock the parent's blocked reads by closing the parent's **read** handles (POSIX `close` of the fd, Windows `CloseHandle` on the pipe / `CancelSynchronousIo`) — this makes the blocked read return with an error/EOF; (3) `WaitFor`/`Kill` the child; (4) `join` the reader threads **before** destroying the `Process`/`FrameChannel` so no thread touches freed state (no use-after-free). Reader threads MUST treat a closed-handle read as a normal stop, never as a protocol error, and MUST NOT outlive the channel. `Kill()` must be safe to call from any thread and idempotent.
- Rationale: a crashed cook must be an observable failure, not a hang; `asset-pipeline` D3/D14 require FAILED (no source fallback) and clean restart. The reader-thread contract prevents both shutdown hangs and use-after-free when the channel is torn down.

### D8. Code structure and module placement (AGENTS.md)

Follow the existing repo layout: `Framework` globs `src/*`, `include/*`, and the per-OS `platform/<os>/*` dirs (`engine/framework/CMakeLists.txt:3-16`), so new files are picked up without editing CMake except where a new platform branch is needed. The `Process` primitive sits next to `RunCmd` (framework platform layer); the generic transport is a small reusable `framework/ipc` subdir; the cook-specific protocol and runners stay in `framework/asset`; the worker host is a tool. No plugin is involved, so no module links anything plugin-level.

New / changed files:

```
engine/framework/
|-- include/framework/platform/
|   `-- Process.h                 [NEW]  ProcessDesc, ProcessStatus, IProcess, CreateProcess()
|-- include/framework/ipc/
|   `-- FrameChannel.h            [NEW]  IFrameChannel, FrameChannel (u32 LE len + UTF-8 JSON over IProcess)
|-- include/framework/asset/
|   |-- ICookRunner.h             [NEW]  ICookRunner, CookJob
|   |-- InProcessCookRunner.h     [NEW]  wraps CookWorker on the cook pool
|   |-- OutOfProcessCookRunner.h  [NEW]  worker client: handshake, request/result, timeout/restart
|   `-- CookProtocol.h            [NEW]  hello/ready/cook/result/ping/pong/shutdown message structs + codec
|-- src/platform/
|   `-- Process.cpp               [NEW]  platform-selecting factory (returns concrete impl)
|-- src/ipc/
|   `-- FrameChannel.cpp          [NEW]  framing, partial read/write loops, reader threads, stderr pump
|-- src/asset/
|   |-- InProcessCookRunner.cpp   [NEW]  routes IAssetEvent::OnAssetBuildFinished
|   |-- OutOfProcessCookRunner.cpp[NEW]  spawn + FrameChannel + in-flight table
|   |-- CookProtocol.cpp          [NEW]  rapidjson (de)serialization of the message schema
|   `-- AssetManager.cpp          [EDIT] on-demand cook dispatches through ICookRunner (D5)
|-- platform/windows/
|   `-- Win32Process.h/.cpp       [NEW]  CreateProcessW + CreatePipe + job object + TerminateProcess
|-- platform/posix/
|   `-- PosixProcess.h/.cpp       [NEW]  posix_spawn + pipe+fcntl(CLOEXEC) + waitpid(WNOHANG) + kill
|                                        (portable subset; compiled for BOTH Darwin and Linux)
|-- platform/linux/
|   `-- LinuxPlatform.h/.cpp      [NEW]  headless PlatformBase + Platform::Init (PlatformType::Linux)
`-- platform/android/
    `-- (Process.cpp factory returns "unsupported"; no impl file)

tools/
|-- CMakeLists.txt                [EDIT] add_subdirectory(asset_tool) OUTSIDE the SKY_BUILD_TOOL gate
`-- asset_tool/
    |-- CMakeLists.txt             [NEW]  sky_add_exe(TARGET AssetTool ... LIBS Framework)
    |-- main.cpp                   [NEW]  parse mounts/platform, build FS+AssetManager, ready handshake
    `-- CookWorkerHost.h/.cpp      [NEW]  serve cook frames via CookWorker, emit result frames, shutdown
```

Placement rationale:
- **`Process.h` in `include/framework/platform/`** matches the existing `PlatformBase.h`; the concrete impls live in `platform/<os>/` like `Win32Platform.*`. `platform/posix/` holds the portable POSIX subset compiled for **both macOS and Linux** (a single source, no per-OS fork).
- **`framework/ipc/FrameChannel`** is transport-generic (no asset types), so it is reusable and keeps the asset module free of raw pipe handling.
- **`CookProtocol.h`** is asset-cook-specific (knows uuid/target), so it stays under `asset/`.
- **`OutOfProcessCookRunner`** lands in `Framework` (it depends only on `Process` + `FrameChannel` + `CookWorker`); it can move to a plugin bridge later without changing the `ICookRunner` interface.
- **`AssetTool`** is a tool executable (`sky_add_exe`), built independent of `SKY_BUILD_TOOL`; it links `Framework` and reuses `CookWorker`.

CMake touch points:
- `engine/framework/CMakeLists.txt`: add `platform/posix/*` to the existing `Darwin` branch, and add a `Linux` branch (`platform/posix/*` + `platform/linux/*`, plus the Linux libs from `cmake/configuration.cmake`). `src/*` and `include/*` are already globbed, so the new `src/ipc` / `src/asset` files need no edit.
- `engine/core/CMakeLists.txt`: add a `Linux` branch reusing `platform/unix/async` (as Android does).
- `engine/CMakeLists.txt`: on Linux, build only the subset needed for the worker (`core`, `framework`, `launcher` optional) and skip render/aurora/editor/etc. for now (see D9).
- `tools/CMakeLists.txt`: add `add_subdirectory(asset_tool)` before/outside `if (SKY_BUILD_TOOL)`.
- On Android/iOS (and any platform with no `Process` backend) the factory in `src/platform/Process.cpp` returns an unsupported status; no platform impl is compiled.

### D9. Linux: wiring only, real cook deferred (lower priority)

Linux has **no engine build branch at all** (no `Linux` in any `CMakeLists.txt`/`.cmake`; `engine/core`, `engine/framework`, `engine/launcher` branch only on `Windows`/`Darwin`/`Android`; CI is Windows-only). The POSIX `Process` compiles there, but `AssetTool` links `Framework`, so `Core` + `Framework` must build on Linux first.

**Critical limitation (verified):** linking `Framework` provides **zero asset builders**. Builders are registered only from dynamically loaded modules (`SkyRender.Builder` in `engine/render/builder/module/BuilderModule.cpp:42-49`, plus `SkyAudio.Builder`/`SkyNavigation.Builder`/`SkyTerrain.Builder`/`Aurora.Cook`), and `SkyRender.Builder` pulls `RenderBuilder.Static` + the shader compiler (`ShaderCompiler.h`, glslang/dxc). A Linux build of only `Core`+`Framework` therefore **cannot cook anything**.

Decision: this change's Linux deliverable is the **wiring and the process primitive only** — `platform/posix` (shared with macOS) + a minimal headless `LinuxPlatform` + CMake so `Core`, `Framework`, and `AssetTool` configure, build, and start, completing the `hello`/`ready` handshake. **Real out-of-process cook on Linux is deferred** to a follow-up once the builder modules build on Linux; it is explicitly out of scope here.

- **CMake config**: add a `Linux` branch to `cmake/configuration.cmake` (link `pthread`/`dl`), `engine/core/CMakeLists.txt` (reuse `platform/unix/async`), `engine/framework/CMakeLists.txt` (compile `platform/posix/*` + `platform/linux/*`).
- **Headless platform**: `engine/framework/platform/linux/LinuxPlatform.{h,cpp}` implements `PlatformBase` (`GetInternalPath`/`GetBundlePath` from `/proc/self/exe`, `GetEnvVariable`, `PlatformType::Linux`) and `Platform::Init`; no window.
- **Module gating**: `engine/CMakeLists.txt` builds only `core`, `framework` (and `tools`) on Linux; render/aurora/editor/sandbox and the rest are skipped.
- **Priority**: Windows and macOS land first and are fully functional; Linux tasks are lower priority and can trail. Setting `cook.mode = "out-of-process"` on Linux without builders fails the request (never strands a `LOADING` asset), which is the correct, observable behavior until builders land.
- A windowed SDL/genetic Linux editor and the full engine port are out of scope.

Rationale: the process/pipe code is already portable; Linux's real blocker is the builder/toolchain port, which is a separate, much larger effort. Splitting it keeps this IPC change bounded and honest about what Linux can actually do.

### D10. Worker bootstrap parity (verified hazard)

The worker cannot just call `ToolApplicationBase` and cook. Verified: `Application`/`ToolApplicationBase` do **not** set up the mount namespace, source catalog, `AssetBuilderManager` filesystems (cook config + `product.index` bundles), or call `AssetDataBase::Load()`; those live in the Qt editor's `EditorApplication::Init` (`engine/editor/src/application/EditorApplication.cpp:54-82`) plus `AssetDataBase::Load()`. Also, linking `Framework` yields **no builders** — they register only from module `Init` (`RegisterBuilder`), and `ToolApplicationBase` reads a `config/modules_tool.json` that does not exist in the repo.

The worker `main` therefore MUST, in order:
1. Redirect stdout (D3) **before** any engine init.
2. `Platform::Init`.
3. Build the mount namespace (`MultiFileSystem`) and pass the same roots/target the editor sends.
4. `AssetDataBase::SetEngineFs`/`SetWorkSpaceFs`, `AssetManager::SetSourceCatalog`, and `AssetBuilderManager::SetEngineFs/SetWorkSpaceFs/SetInterMediateFs` (loads the cook config; falls back to `configs/asset_build_presets.json` when `asset_cook.jsonc` is absent).
5. Install reflection/type registration and **load the builder modules** (a worker module config or an explicit `RegisterModule` list mirroring the editor's `SkyRender.Builder`/etc.). `RebuildCacheFromScan` needs the builders registered **before** it runs.
6. `AssetDataBase::Load()` (or a scan) so the source catalog is populated.
7. Complete the `hello`/`ready` handshake, then serve requests.

This parity step is the largest hidden cost of the change and is its own task group.

### D11. Index visibility and single writer

- **Editor refresh (verified gap)**: `AssetManager` reads a bundle's `product.index` only at `AddAssetProductBundle` (`engine/framework/src/asset/AssetManager.cpp:51-61`) and never re-reads it (`AssetIndexFileCache::Invalidate` has no production caller). An out-of-process cook writes the index in the **worker**, so the editor's `productPathMap` stays stale and later path loads miss. The completion handler MUST refresh the editor's mapping for the cooked `path → uuid` (a narrow `AssetManager` refresh API, or re-parse/re-read the target bundle's index) **before** fulfilling the pending load.
- **Single writer**: in out-of-process mode only the worker writes `product.index`/manifests, and the editor MUST NOT run concurrent in-process cooks into the same bundle. This invariant is documented and asserted; cross-process OS file locking is deferred (no file-lock helper exists in the repo). Concurrent cooks **within** the worker remain serialized by the existing per-bundle in-process lock + atomic temp+rename.

## Risks / Trade-offs

- **stdout/log collision** (verified: `LOG_*` → `printf` → stdout) → the worker redirects fd 1 → 2 before init and writes frames to a dup'd private fd (D3); covered by a "logs never appear on the frame channel" test.
- **Worker bootstrap drift** (verified: `Framework` alone has no builders; mounts/catalog/cook config not set by `ToolApplicationBase`) → replicate the editor boot explicitly (D10); test the worker end-to-end, not just the runner.
- **Editor index staleness** (verified: no `product.index` re-read after cook) → refresh the editor mapping on completion before fulfilling the load (D11).
- **Cross-process index/manifest races** → out-of-process mode has a single writer (the worker) and the editor does not cook in-process concurrently; OS file locking deferred (D11).
- **Linux cannot cook** (verified: builders and their toolchain are not built) → Linux deliverable is wiring + handshake only; real cook deferred (D9). If `out-of-process` is selected on Linux, requests fail observably.
- **Cross-platform spawn differences** (Windows handle inheritance and pipe semantics vs POSIX fd hygiene) → keep the surface tiny; per-OS implementations with focused unit tests; fail fast on mobile.
- **Frame desynchronization** (a stray write to stdout by a dependency) → stdout is frames-only by contract, logs forced to stderr, a max-frame cap detects desync, and the reader validates length before reading the body.
- **Deadlock on full pipe buffers** → dedicated stdout/stderr reader threads run continuously; the parent never writes a large payload while the child's stdout is unread; stdin writes use write loops and handle a closed pipe as a dead worker.
- **Reader-thread teardown differs per OS** (POSIX `close` unblocks `read`; Windows needs `CloseHandle`/`CancelSynchronousIo`) → the fixed shutdown order in D7 plus a test that drain/restart joins both reader threads with no leak or hang.
- **`fork`+`execve` fallback hazards** (async-signal-safety in a multithreaded process) → pre-built buffers and async-signal-safe calls only, prefer `posix_spawn`+`addchdir_np`, and a test that exercises the fallback path on a platform without `addchdir_np` support.
- **Zombie/orphan processes** → `WaitFor`/`Kill` on timeout and a `shutdown` frame on `Drain`; kill-on-parent-exit is only available on Windows (job object). macOS has no parent-death signal, so cleanup relies on explicit `Drain`/`Kill` (and a future Linux backend may add `prctl(PR_SET_PDEATHSIG)`).
- **Worker drift from the editor** (different mounts/config) → pass the mount namespace and platform target explicitly and verify them in the `ready` handshake.
- **Protocol versioning** → the `hello`/`ready` handshake rejects a mismatched protocol; the version is bumped on any wire change.
- **Scope creep into a framework** → keep the transport request/response-only; no multiplexed streams, no auth, no remote (non-local) transport.
- **Linux scope creep** → Linux is not a full target: only the headless worker subset is built; windowed editor/rendering stays skipped by module gating, and Linux tasks are lower priority.
- **OS-specific error handling drift** → tests for one process backend run on every desktop OS the CI can build; `posix` is shared so macOS and Linux exercise the same code (accepting that Linux is tested later).

## Migration Plan

1. Land the `Process` primitive for Windows and the shared POSIX backend (macOS) with pipe + wait/kill and unit tests.
2. Land the frame codec (`FrameChannel`) with partial-frame/oversize tests.
3. Add `ICookRunner` + `InProcessCookRunner` and route the existing in-process on-demand cook through it (behavior-preserving; no config change).
4. Add `tools/asset_tool` with the **fd redirection (D3)** and the **full worker bootstrap (D10)**: mounts, source catalog, `AssetBuilderManager` FS/cook config, reflection, builder-module loading, `AssetDataBase::Load()`, then the `ready` handshake. Reuse `CookWorker`.
5. Add `OutOfProcessCookRunner` (handshake, cook/result, correlation, timeout, restart) and the `cook.mode` config selector; default stays `in-process`.
6. Add the **editor product-index refresh (D11)** on completion, and enforce the single-writer invariant.
7. Add integration tests: missing product → out-of-process cook → LOADED; worker crash/timeout → FAILED + restart; stderr logs never appear on stdout; the editor resolves a path after an out-of-process cook.
8. (Lower priority) Add the minimal Linux backend (D9): CMake branches, headless `LinuxPlatform`, `engine/` module gating, then configure/build `AssetTool` and verify start + handshake (real cook deferred).

Rollback: set `cook.mode = "in-process"` (or remove the selector) to fall back to the existing path; the `Process`/codec modules are additive and unused when out-of-process is off. The Linux backend tasks can be dropped without affecting Windows/macOS.

## Open Questions

- Default per-request timeout value and whether idle workers auto-exit after a quiet period.
- Whether `Platform::RunCmd` should be reimplemented on `Process` now or left for a follow-up.
- Exact `AssetTool` CLI/env contract for mount namespace transfer (argv vs a small config/env block).
