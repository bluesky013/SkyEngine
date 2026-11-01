> 2026-11-01 整理：**superseded/history-only**。现行 owner：现行 acquire/submit/retire 归 renderer coordinator；未验 editor 恢复场景归 `aurora-renderer-platform-parity` P02-EDITOR。
> 剩余：旧 3.3 保留未验收转交；禁止恢复 Editor 直接 reset fence/combined-submit 协议。下方较早状态与验收记录保留为历史。

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
- [ ] 3.3 Transferred, not accepted: `aurora-renderer-platform-parity` P02-EDITOR owns native minimize/restore, unavailable/out-of-date/acquire failure and close/shutdown recovery; coordinator-owned completion replaces the historical editor fence protocol.

## Verification record (2026-10-16)

- Audited all three Queue::Submit implementations and ClientViewport synchronization locally; no backend/interface changes were made.
- The coordinating build compiled EditorRender and EditorRenderTest. All 6 EditorRenderTest CPU regressions passed in the normal build and all 6 under AddressSanitizer. The set contains this change's 3 real-frame-context/mock-queue tests and the surface-teardown change's 3 event-lifetime tests; counts are shared between the changes, not separate sets.
- Strict OpenSpec validation and scoped `git diff --check` passed.
- The renderer remains serialized on the editor thread. T02 retains surfaces through the new scope's submit/present completion.
- The initial bounded Metal editor smoke failed with SIGBUS (exit -10). Crash registers, host disassembly, and the loaded Bullet image's vtable identify a stale pre-RegisterMeta IModule ABI; no renderer/frame-scope function appears in the faulting stack. This failed run remains recorded separately from the subsequent startup/exit result.
- The coordinating full build (`cmake --build build --parallel 4`, 263 steps) succeeded and rebuilt the runtime modules. A rerun with `./output/bin/SandboxEditor --console --frames 4 --rhi Metal` exited 0 and no longer hit the Bullet PostTick crash. The log explicitly reports `MTLCreateSystemDefaultDevice failed` / `aurora device init failed`, so no SandboxModule renderer was established and no native frame/fence/presentation behavior was verified. This is a module startup/exit smoke only. Historical duplicate Cocoa-class warnings remain a separate architecture item. Log: `/private/tmp/sky-ui-native-smoke-rebuilt.log`.
- Native GPU/backend acceptance and Win32/VS2022 verification remain pending; CPU models cannot verify native presentation/image layout or full device-loss recovery.
- No commit or OpenSpec archive was performed.

## 2026-11-01 历史协议退役

当前 `EditorRendererWindows.cpp` 经 `RendererHost` 提交 owned `FramePacket`，由 `EditorViewportState` 发出 DestroyViewport 并等待精确 `ViewportDestroyed` 确认；editor 不再直接驱动 DeviceFrameContext fence、acquire semaphore 或 combined native submission。此 change 保留历史修复/失败证据，申请 history-only 归档，不把旧实现约束同步回主 spec。未执行的真实 editor 恢复/浮窗场景由 P02-EDITOR 明确接收。
