## 1. Backend and ownership audit

- [x] 1.1 Verify Vulkan, DX12, and Metal support a fence-bearing submission with no caller command buffers.
- [x] 1.2 Audit ClientViewport acquire/render-done/present obligations and preserve the frame wait before swapchain maintenance.

## 2. Implementation

- [x] 2.1 Add a local frame-submission scope that pairs every BeginFrame with one fence submission and EndFrame, including early exits.
- [x] 2.2 Register every successful main/preview/floating acquisition and preserve its synchronization/presentation obligations even without a usable backbuffer.
- [x] 2.3 Preserve one combined frame submission, frame-held surface ownership, and conservative acquire stage coverage for preview transfer work.
- [x] 2.4 Add CPU mock regressions using the real frame-context fence logic for skipped-frame recovery, acquired-but-unusable images, and combined windows.

## 3. Verification

- [x] 3.1 Build EditorRender and EditorRenderTest and run regressions (including ASan where available).
- [x] 3.2 Run strict OpenSpec validation and scoped whitespace checks.
- [ ] 3.3 Run native GPU minimize/restore, unavailable/out-of-date/acquire-failure, acquired-image recovery where reproducible, and close/Shutdown recovery checks. Keep this separate from CPU/build completion.

## Verification record (2026-10-16)

- Audited all three Queue::Submit implementations and ClientViewport synchronization locally; no backend/interface changes were made.
- The coordinating build compiled EditorRender and EditorRenderTest. All 6 EditorRenderTest CPU regressions passed in the normal build and all 6 under AddressSanitizer. The set contains this change's 3 real-frame-context/mock-queue tests and the surface-teardown change's 3 event-lifetime tests; counts are shared between the changes, not separate sets.
- Strict OpenSpec validation and scoped `git diff --check` passed.
- The renderer remains serialized on the editor thread. T02 retains surfaces through the new scope's submit/present completion.
- The initial bounded Metal editor smoke failed with SIGBUS (exit -10). Crash registers, host disassembly, and the loaded Bullet image's vtable identify a stale pre-RegisterMeta IModule ABI; no renderer/frame-scope function appears in the faulting stack. This failed run remains recorded separately from the subsequent startup/exit result.
- The coordinating full build (`cmake --build build --parallel 4`, 263 steps) succeeded and rebuilt the runtime modules. A rerun with `./output/bin/SandboxEditor --console --frames 4 --rhi Metal` exited 0 and no longer hit the Bullet PostTick crash. The log explicitly reports `MTLCreateSystemDefaultDevice failed` / `aurora device init failed`, so no SandboxModule renderer was established and no native frame/fence/presentation behavior was verified. This is a module startup/exit smoke only. Historical duplicate Cocoa-class warnings remain a separate architecture item. Log: `/private/tmp/sky-ui-native-smoke-rebuilt.log`.
- Native GPU/backend acceptance and Win32/VS2022 verification remain pending; CPU models cannot verify native presentation/image layout or full device-loss recovery.
- No commit or OpenSpec archive was performed.
