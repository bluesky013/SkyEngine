## Why

The active `asset-pipeline` change specifies an **out-of-process cook mode** (`ICookRunner` + `OutOfProcessCookRunner`, an `AssetTool` worker host, and a stdout length-prefixed IPC protocol) but expressly deferred its implementation (`tasks.md` 10.4, 10.5, 10.6c, 14.4, 14.5; `design.md` D7/D14). As a result, on-demand cook today can only run **in-process**: a long cook shares the editor's address space and CPU, cannot be restarted after a crash, and cannot be isolated from an editor bug.

The scaffolding has no foundation to build on. The engine ships **no child-process or IPC facility** — the only process call is a blocking, read-only, Windows-only `Platform::RunCmd` wrapper around `_popen` (`engine/framework/platform/windows/Win32Platform.cpp:153-165`). There is no `CreateProcess`/`posix_spawn`, no pipe, no stdin write, and no way to correlate a worker response with a request.

This change builds that foundation: a thin, self-developed process + pipe primitive and a length-prefixed frame protocol, plus the `ICookRunner` seam and `AssetTool` worker host that `asset-pipeline` left deferred. It is deliberately **not** a general-purpose RPC/IPC framework.

## What Changes

- Add a **platform-layer process/pipe primitive** — start a child with explicit argv/env/cwd, write its stdin, read its stdout and stderr, wait with a timeout, and kill it — for desktop **Windows** (`CreateProcess` + anonymous pipes) and a single shared **POSIX** backend (`posix_spawn` + `pipe()`/`fcntl(FD_CLOEXEC)` + `waitpid`/`kill`; no `pipe2`, no parent-death signal) serving **macOS and Linux**. Mobile (Android/iOS) reports **unsupported**; the runtime never cooks.
- Add a **minimal Linux build backend** so `AssetTool` can build, start, and handshake on Linux: Linux branches for the `Core`/`Framework` config, reuse of `core/platform/unix/async`, a headless `LinuxPlatform`, and gating `engine/` to `Core` + `Framework` + `AssetTool`. **Linux is lower priority** and **real Linux cook is deferred** — linking `Framework` provides no asset builders (they live in dynamically loaded builder modules), so a Linux-only build cannot cook until those builders/toolchain build on Linux in a follow-up. A windowed Linux editor is out of scope.
- Add a **length-prefixed frame codec**: a little-endian `u32` length header followed by a UTF-8 JSON payload. stdout carries **only** frames; stderr carries **logs**. Partial reads/writes and oversized/malformed frames are handled; cancellation drains the pipe.
- Add the **`ICookRunner` seam** in `engine/framework/asset`: `InProcessCookRunner` (wraps the existing `CookWorker` on the cook pool) and `OutOfProcessCookRunner` (spawns the `AssetTool` worker, sends `{uuid,target,path}`, matches responses by `uuid`+`target`, and raises the **same** `IAssetEvent::OnAssetBuildFinished` completion event so the loading layer is mode-agnostic).
- Add the **builder-side `AssetTool` worker host** executable (built **independent of `SKY_BUILD_TOOL`**) that reuses `CookWorker` to drain requests and speaks the protocol; it starts with the **same mount namespace and platform target** as the editor and reports results through the protocol frames.
- **Worker boot parity**: the worker MUST replicate the editor's initialization (mounts, source catalog, `AssetBuilderManager` filesystems/cook config, reflection, and **loading the builder modules** — `Framework` alone registers no builders) and route engine logs to stderr before init. The root cause is fixed in `Logger` (new `Logger::SetOutputStream`, default stdout unchanged; the worker selects stderr), with fd redirection kept as defense-in-depth against stray non-`Logger` stdout writers.
- **Editor index refresh**: after an out-of-process cook, the editor MUST refresh its in-memory product mapping for the cooked path, since it otherwise only reads `product.index` at bundle registration; out-of-process mode also uses the worker as the **single writer** of the bundle index/manifests.
- Add **worker lifecycle**: one persistent worker per session, a request queue matched by `uuid`/`target`, per-request **timeout**, and crash/timeout handling that **fails the request** (never strands a `LOADING` asset) and restarts the worker on demand.
- Add **mode selection** through cook config (in-process vs out-of-process), defaulting to in-process.
- **Out of scope**: changing the reference/identity/loading model (owned by `asset-pipeline`), incremental/freshness build, out-of-process cook on mobile, a full windowed Linux engine/editor port, and **real Linux cook** (deferred until the builder modules/toolchain build on Linux; this change delivers Linux wiring + handshake only).

## Capabilities

### New Capabilities

- `native-process`: desktop child-process spawn with stdin/stdout/stderr pipes and lifecycle control (wait-with-timeout, kill) on Windows, macOS, and Linux, plus the "unsupported on mobile" contract.
- `asset-cook-ipc`: the length-prefixed frame codec, the `ICookRunner` in-process/out-of-process seam, the `AssetTool` worker host protocol and boot parity (builder-module loading, stdout redirection), response correlation by `uuid`/`target`, log separation, post-cook editor index refresh, single-writer safety, and worker timeout/crash/restart semantics.
- `linux-platform-backend`: the minimal Linux build/platform support (CMake branches, unix async reuse, headless `LinuxPlatform`, `engine/` subset gating) that lets `Core` + `Framework` + `AssetTool` configure, build, and handshake on Linux; real Linux cook is deferred until the builder modules build there.

### Modified Capabilities

- (none)

## Impact

- `engine/framework/platform/` (and per-OS backends `platform/{windows,posix,linux}`): add a `Process` primitive next to `RunCmd`; `Platform::RunCmd` may be reimplemented on top of it.
- `engine/framework/asset/`: add `ICookRunner`/`InProcessCookRunner`/`OutOfProcessCookRunner` and the frame codec; the loading layer keeps depending only on `IAssetEvent`; `AssetManager` gains a narrow product-index refresh so the editor sees an out-of-process cook.
- Worker bootstrap touches the module system: `AssetTool` must load the builder modules (via a worker module config or explicit `RegisterModule` list) to have builders.
- `cmake/configuration.cmake`, `engine/{core,framework,launcher}/CMakeLists.txt`, `engine/CMakeLists.txt`: add Linux branches and gate `engine/` to the subset needed for the worker (Linux is lower priority).
- `engine/framework/platform/linux/LinuxPlatform.{h,cpp}` (new): a headless `PlatformBase` + `Platform::Init` for Linux.
- `tools/asset_tool/` (new): the worker-host executable and its `CMakeLists.txt`; wire it into `tools/` independent of `SKY_BUILD_TOOL`.
- Cook config: a mode selector (in-process vs out-of-process) and the worker executable path.
- Tests: `FrameworkTest`/`AssetManagerTest` for the frame codec, process pipes, response correlation, timeout/crash, and log separation.
- Depends on `asset-pipeline` task 10.1 (`AssetBuildResult` carrying `uuid`/`target`/`retCode`/`error`), which is already implemented.
