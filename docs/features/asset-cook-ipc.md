---
title: "Asset Cook IPC (Out-of-Process Cook)"
description: "In-process vs out-of-process asset cooking: the process/pipe primitive, the length-prefixed frame protocol, the AssetTool worker host, and worker lifecycle."
module: "framework"
updated: "2026-10-03"
---

## Overview

Cooking turns source assets into per-platform products (see `asset-pipeline.md`). Cooking can run in two modes,
selected by cook configuration:

- **In-process** (default): the builder runs on the engine's asset/cook pool inside the current process.
- **Out-of-process**: a separate `AssetTool` worker process performs the cook; the host talks to it over a
  length-prefixed frame protocol on the worker's stdio.

The loading layer is mode-agnostic: it depends only on `ICookRunner` and `IAssetEvent`, so both modes raise the same
`OnAssetBuildFinished` completion event.

Out-of-process cooking is a **desktop-only** feature (Windows, macOS, Linux). Mobile has no cook and no child
processes.

## Enabling out-of-process mode

`configs/asset_cook.jsonc` may carry a `cook` block:

```jsonc
{
  "cook": {
    "mode": "out-of-process",          // "in-process" | "out-of-process"
    "worker": {
      "path": "AssetTool.exe",         // optional; defaults to a sibling AssetTool[.exe]
      "timeoutMs": 600000              // optional per-request timeout
    }
  }
}
```

`AssetBuilderManager::SetWorkSpaceFs` parses this and, when out-of-process is selected, creates an
`OutOfProcessCookRunner` and hands it to `AssetManager::SetCookRunner`. When unset, the built-in inline in-process
path is used unchanged.

## Process primitive

`framework/platform/Process.h` defines `IProcess` and a `CreateProcess()` factory:

- `Start(ProcessDesc{argv, env, cwd})` launches a child with piped stdio.
- `WriteStdin` / `ReadStdout` / `ReadStderr`; reads block for at most a short interval so a reader loop can observe
  shutdown without a cancel API.
- `WaitFor(timeout, exitCode)`, `Kill()` (idempotent, any thread), `IsRunning()`.

Backends:

- **Windows**: `CreateProcessW` + anonymous pipes, a Job Object (`KILL_ON_JOB_CLOSE`), `TerminateProcess`.
- **POSIX (macOS/Linux, shared source)**: `posix_spawn` + `pipe()`/`fcntl(CLOEXEC)` (no `pipe2`), `POSIX_SPAWN_SETPGROUP`
  + `kill(-pgid)`, `waitpid(WNOHANG)` polling, `SIGPIPE` ignored. Working directory uses `addchdir_np` when available,
  otherwise an async-signal-safe `fork`+`execve` fallback.
- **Mobile / unknown**: an unsupported stub; `Start` fails explicitly.

## Frame protocol

`framework/ipc/FrameChannel` frames bytes as `[u32 little-endian length][payload]`. The frame stream carries protocol
frames only; the optional log stream carries text lines. A maximum frame size rejects a desynchronized channel.

The cook message schema (`framework/asset/CookProtocol.h`) is JSON with a `type` discriminator:

| Message | Fields |
|---|---|
| `hello` | `protocol` |
| `ready` | `protocol`, `platform` |
| `cook` | `id`, `uuid`, `target`, `path` |
| `result` | `id`, `uuid`, `target`, `retCode`, `error` |
| `ping` / `pong` / `shutdown` | — |

Results are matched to in-flight requests by `id`; `uuid` and `target` are echoed through to the completion event.

## Worker host (`AssetTool`)

`tools/asset_tool` is built independently of `SKY_BUILD_TOOL`. Its bootstrap mirrors the editor:

1. Route engine logs off stdout: `Logger::SetOutputStream(stderr)`, plus a defensive dup of stdout to a private frame
   fd with fd 1 redirected to stderr.
2. `Platform::Init`, build the mount namespace (`--project`/`--engine`/`--intermediate`), set the source catalog and
   `AssetBuilderManager` filesystems (loading the cook config).
3. Register and load the **builder modules** (`SkyRender.Builder`, `SkyAudio.Builder`, …); linking the framework alone
   registers no builders. Missing module DLLs are skipped, and the affected cooks then fail observably.
4. `AssetDataBase::Load()`, then serve `cook` frames via `CookWorker::CookBatch`, emitting `result` frames.

## Lifecycle

- One persistent worker per session; requests matched by `id`.
- A per-request timeout, an unexpected worker exit, or a protocol failure fails **all** in-flight requests (raising a
  failed completion so no `LOADING` asset is stranded), kills the worker, and allows a fresh worker on the next request.
- `Drain()` sends `shutdown`, waits, then kills, leaving no orphan process.

## Single-writer invariant

In out-of-process mode the **worker is the only writer** of a bundle's `product.index`/manifests. The editor must not
run concurrent in-process cooks into the same bundle; `AssetBuilderManager` asserts this. Cross-process file locking
is not implemented; the invariant is enforced by keeping a single writer.

When the cook completes, the editor refreshes its in-memory product mapping (`AssetManager::RefreshProductIndex`) so
path lookups resolve against the freshly written index before the pending load is fulfilled.

## Platform status

| Platform | Worker build | Cook |
|---|---|---|
| Windows | yes | yes |
| macOS | yes (shared POSIX) | yes |
| Linux | wiring + handshake only | deferred until builder modules build on Linux |
| Android / iOS | no | no (unsupported) |

## Tests

`FrameworkTest` covers the process primitive (`ProcessTest`), the frame codec (`FrameChannelTest`), and the protocol
(`CookProtocolTest`); the existing asset tests confirm the default in-process path is unchanged.
