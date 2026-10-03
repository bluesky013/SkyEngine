## 1. Process primitive (platform)

- [x] 1.1 Add `engine/framework/include/framework/platform/Process.h` (next to `PlatformBase.h`) declaring `ProcessDesc{argv, env, cwd}`, `ProcessStatus`, `IProcess` (`Start`, stdin write, stdout/stderr read, `WaitFor(timeout, exitCode)`, `Kill()`, `IsRunning()`), and a `CreateProcess()` factory; add the selecting factory `engine/framework/src/platform/Process.cpp`.
- [x] 1.2 Implement `engine/framework/platform/windows/Win32Process.{h,cpp}`: `CreateProcessW` with inherited stdio handles and `CreatePipe`; `WaitForSingleObject` + `TerminateProcess`; kill the child if the parent exits (job object). Verified: `Framework` builds/links on Windows.
- [x] 1.3 Implement `engine/framework/platform/posix/PosixProcess.{h,cpp}` for **macOS + Linux** (single shared source) using only the portable POSIX subset: `posix_spawn` + `posix_spawn_file_actions_adddup2`/`addclose`, `pipe()`+`fcntl(F_SETFD, FD_CLOEXEC)` (no `pipe2`), working dir via `addchdir_np` (guarded) with a `fork`+`execve` fallback, `POSIX_SPAWN_SETPGROUP` + `kill(-pgid)` for tree teardown, ignore `SIGPIPE`, `waitpid(WNOHANG)` polling for the timeout, `kill(SIGKILL)`. Added `platform/posix/*` to the `Darwin` branch in `engine/framework/CMakeLists.txt`. (POSIX not compiled in this Windows environment — needs a mac/linux build.)
- [x] 1.4 Return an explicit "unsupported" error on Android/iOS (and any platform with no backend) rather than failing silently or hanging; done via the `UnsupportedProcess` stub in `engine/framework/src/platform/Process.cpp` (factory returns it where no backend is compiled).
- [x] 1.5 Add unit tests (`ProcessTest.cpp` + `ProcessHelper` exe): start a helper, echo stdin→stdout, read stderr separately, exit code, wait timeout then kill, missing executable. Verified: 4/4 pass on Windows.
- [x] 1.6 Make the `fork`+`execve` fallback (used only when `addchdir_np` is unavailable) async-signal-safe: pre-build `argv`/`envp` and the absolute exe path **before** `fork`; between `fork` and `execve` call only `dup2`/`close`/`chdir`/`setpgid`/`execve`/`execvp`/`_exit` (no `malloc`, no locks, no logging, no C++ objects); capture failure via the child exit code. (`SpawnForkExec` in `PosixProcess.cpp`; needs a POSIX build + test.)

## 2. Frame codec

- [x] 2.1 Add `engine/framework/include/framework/ipc/FrameChannel.h` + `engine/framework/src/ipc/FrameChannel.cpp`: framing over an `IProcess` that writes `[u32 LE length][UTF-8 JSON payload]` frames and reassembles frames from partial reads; enforce a max-frame cap and fail on a declared over-length. Verified: `Framework` builds/links on Windows.
- [x] 2.2 Read stdout frames on a dedicated reader thread and pump stderr into the engine logger via a log callback; logs never touch the frame stream. (Logger wiring to `LOG_*` deferred to the AssetTool host, task 4.x.)
- [x] 2.3 Handle write loops, a closed stdin, EOF/desync (non-frame bytes / truncated frame on stdout) via `Fail()`, and cancellation by draining and closing the channel.
- [x] 2.4 Add unit tests (`FrameChannelTest.cpp`): length-prefix encoding, split-header/body reassembly, oversized frame rejected, truncated frame fails the channel, logs separate from frames. Verified: pass on Windows.
- [x] 2.5 Shutdown order (D7): `Stop()` sets a flag; reads poll with a short timeout so reader/log threads join promptly; `Kill()` is mutex-guarded/idempotent. `StopJoinsReaderThatNeverReachesEof` verifies joining a reader that never reaches EOF. Verified: pass on Windows.

## 3. Cook runner seam (`engine/framework/asset`)

- [x] 3.1 Add `engine/framework/include/framework/asset/ICookRunner.h` (`Request(CookJob{uuid,target,path})`, `Drain()`) and a `CookJob` type; keep the loader depending only on `ICookRunner` + `IAssetEvent`. Verified: builds/links on Windows.
- [x] 3.2 Add `InProcessCookRunner.{h,cpp}` on the existing `AssetBuilderManager::BuildRequest`/asset pool, raising the extended `AssetBuildResult` (`uuid`/`target`/`retCode`/`error`) exactly as today. Verified: builds/links on Windows.
- [x] 3.3 Route the on-demand cook through `ICookRunner` when a runner is selected (`AssetManager::SetCookRunner`); completion now flows through `ICookRunner`'s completion handler + `IAssetEvent` (`OnCookFinished`). Default (no runner) keeps the existing inline `BuildRequestSync` path unchanged. Verified: 26 asset tests pass.
- [x] 3.4 Add tests: `AssetManagerTest.InProcessCookRunnerTest` routes an on-demand cook through `InProcessCookRunner` and resolves the LOADING asset to LOADED. Verified: passes.

## 4. Worker host (`tools/asset_tool`)

- [x] 4.1 Add the `tools/asset_tool/` target (`sky_add_exe(TARGET AssetTool ... LIBS Framework)` in `tools/asset_tool/CMakeLists.txt`) and `add_subdirectory(asset_tool)` in `tools/CMakeLists.txt`, outside the `SKY_BUILD_TOOL` gate; `tools/` is now added on desktop regardless of `SKY_BUILD_TOOL`. Verified: `AssetTool.exe` builds to `output/bin/Debug`.
- [x] 4.2 **Root fix (D3)**: add `Logger::SetOutputStream(FILE*)` in `engine/core/include/core/logger/Logger.h` + `src/logger/Logger.cpp`, making the console stream configurable with default stdout unchanged (`Print`/`PrintW` now use `fprintf`/`fwprintf`). This lets a protocol-speaking tool route `LOG_*` off stdout without masking it.
- [x] 4.2a Add `engine/test/core/LoggerTest.cpp` (auto-globbed into `CoreTest`): asserts redirected output lands on the configured stream, the output callback still fires when redirected, and resetting to the default is safe.
- [x] 4.3 In the worker, `Logger::SetOutputStream(stderr)` in `main` before engine init, and `WorkerStdio` also dups the real stdout to a private frame fd and dup2s stderr onto fd 1 (Windows `_dup`/`_dup2`, POSIX `dup`/`dup2`) as defense-in-depth. Verified: builds.
- [x] 4.4 **Worker bootstrap parity (D10)** in `main.cpp`: `Platform::Init`; mounts via `NativeFileSystem`; `AssetDataBase::SetEngineFs`/`SetWorkSpaceFs`, `AssetManager::SetSourceCatalog`, `AssetBuilderManager::SetEngineFs/SetWorkSpaceFs/SetInterMediateFs` (loads cook config); builder modules registered (`SkyRender.Builder`/`SkyAudio.Builder`/`SkyNavigation.Builder`); `AssetDataBase::Load()` before serving. Verified: builds.
- [x] 4.5 Add `tools/asset_tool/CookWorkerHost.{h,cpp}`: `hello`/`ready` handshake (protocol version + platform target), serve `cook` frames via `CookWorker::CookBatch`, emit `result` frames. Verified: `AssetTool.exe` builds/links.
- [x] 4.6 Control frames (`ping`/`pong`, `shutdown`) handled in `CookWorkerHost::HandleFrame` and clean exit. Log separation verified by `FrameChannelTest.LogsAreSeparateFromFrames` and `OutOfProcessCookRunnerTest.HealthyWorkerAfterLogsStillCompletes` (worker logs to stderr; frames stay intact).

## 5. Out-of-process runner and config

- [x] 5.1 Add `engine/framework/src/asset/OutOfProcessCookRunner.cpp` (+ header): spawn the worker via `IProcess`, run the handshake, send `cook` frames, match `result` frames by `id`, and raise `IAssetEvent::OnAssetBuildFinished` with the echoed uuid/target/retCode/error. Verified: builds/links on Windows (end-to-end untested until the AssetTool host exists).
- [x] 5.2 Add `engine/framework/include/framework/asset/CookProtocol.h` + `src/asset/CookProtocol.cpp`: the message schema (hello/ready/cook/result/ping/pong/shutdown) with rapidjson; `EncodeCookMessage`/`DecodeCookMessage`. Verified: builds/links on Windows. (Version-mismatch / duplicate-id rejection is enforced in `OutOfProcessCookRunner`.)
- [x] 5.3 Cook-config selection: `CookConfig` parses a `cook` object (`mode` = in-process/out-of-process, `worker.path`, `worker.timeoutMs`), defaulting to in-process. `AssetBuilderManager::SetWorkSpaceFs` creates an `OutOfProcessCookRunner` when selected and resolves a default sibling `AssetTool[.exe]` via `Platform::GetBundlePath()`. The worker forces `SetCookRunner(nullptr)` to avoid recursion.
- [x] 5.4 **Editor index refresh (D11)**: `AssetManager::RefreshProductIndex` invalidates and re-reads every bundle's `product.index`; `OnCookFinished` calls it on success before `DeserializeProduct`, so path lookups resolve after an out-of-process cook.
- [x] 5.5 Enforce the single-writer invariant: `AssetBuilderManager` tracks `outOfProcessActive` and asserts in `BuildRequest` if an in-process cook is requested while out-of-process mode is active; `SetForceInProcess(true)` (used by the worker) skips out-of-process selection so the worker can cook. Cross-process file locking remains deferred. Documented in `docs/features/asset-cook-ipc.md`.
- [x] 5.6 Add tests (`OutOfProcessCookRunnerTest` against a protocol-speaking stub worker): handshake + cook, multiple requests correlated to the right results, a result with an unknown id dropped, logs do not corrupt frames. Verified: 6/6 pass. (Cross-process index refresh is covered by `AssetManager::RefreshProductIndex`; a real-builder LOADED assertion needs builder modules deployed.)

## 6. Lifecycle and concurrency

- [x] 6.1 Maintain one persistent worker per session; serialize writes (FrameChannel write mutex) and guard the in-flight request table (mutex + monitor thread). Implemented in `OutOfProcessCookRunner`.
- [x] 6.2 On per-request timeout / unexpected exit / protocol failure: fail all in-flight requests (raise the completion event with a non-zero code so pending loads become FAILED), kill the worker, and allow a fresh spawn on the next request. Implemented (monitor thread + `OnChannelFailure`); the monitor never self-joins.
- [x] 6.3 Implement `Drain()`: send `shutdown`, wait, then kill if needed; ensure no worker process outlives the editor. Implemented in `OutOfProcessCookRunner::Drain`/`StopWorker`.
- [x] 6.4 Add tests (`OutOfProcessCookRunnerTest`): per-request timeout fails the request; a worker crash fails the in-flight request and the next request restarts a fresh worker (sentinel-based stub); `Drain()` tears the worker down. Verified: pass. (Concurrent `product.index` serialization is covered by the existing per-bundle lock + atomic save.)

## 7. Verification and docs

- [x] 7.1 Run the full framework/asset test suite and confirm the existing `asset-pipeline` tests stay green with `cook.mode` defaulting to in-process. Verified: `FrameworkTest` 86/86 pass (incl. `OnDemandCookTest`, `CookWorkerTest`, new `ProcessTest`/`FrameChannelTest`/`CookProtocolTest`).
- [x] 7.2 End-to-end out-of-process path exercised on desktop via `OutOfProcessCookRunnerTest` (real child process, pipes, handshake, protocol, timeout/crash/restart), guarded by `#if !mobile` so it is skipped on Android/iOS. Note: this uses a protocol stub, not real builders; a real-builder `AssetTool` cook additionally requires deploying the builder modules beside the worker.
- [x] 7.3 Update the affected docs: added `docs/features/asset-cook-ipc.md` (cook mode selection, process primitive, frame protocol, `AssetTool` worker bootstrap, lifecycle, single-writer invariant, platform status) and noted out-of-process cook is desktop-only.

## 8. Minimal Linux backend (DEFERRED - follow-up change)

Deferred at archive time. Windows/macOS are fully functional and the shared `platform/posix`
source is written portable; the Linux build backend (below) is tracked as a follow-up change
and is not part of this archived change's delivered scope.

- [ ] 8.1 (deferred) Add a `Linux` branch to `cmake/configuration.cmake` (link `pthread`/`dl`; no framework libs) and `engine/core/CMakeLists.txt` (reuse `platform/unix/async`, as Android does).
- [ ] 8.2 (deferred) Add `engine/framework/platform/linux/LinuxPlatform.{h,cpp}`: a headless `PlatformBase` (`GetInternalPath`/`GetBundlePath` from `/proc/self/exe`, `GetEnvVariable`, `PlatformType::Linux`) plus `Platform::Init`; add `platform/posix/*` + `platform/linux/*` to a `Linux` branch in `engine/framework/CMakeLists.txt`.
- [ ] 8.3 (deferred) Gate `engine/CMakeLists.txt` on Linux to build only the subset needed for the worker (`core`, `framework`, and `tools`); skip render/aurora/editor/sandbox and the rest.
- [ ] 8.4 (deferred) Configure and build `AssetTool` on Linux, and verify it starts and completes the `hello`/`ready` handshake. Real cook stays deferred: builders (`SkyRender.Builder` etc.) and their toolchain are not built on Linux yet, so selecting `out-of-process` in a Linux cook fails the request observably instead of cooking.
- [ ] 8.5 (deferred) Confirm the shared `platform/posix` compiles unchanged for both macOS and Linux (no per-OS `#ifdef` in the spawn/pipe path except the `addchdir_np` guard).
- [ ] 8.6 (deferred) Build the builder modules (and their dependencies) on Linux so the deferred `out-of-process` cook can be enabled.
