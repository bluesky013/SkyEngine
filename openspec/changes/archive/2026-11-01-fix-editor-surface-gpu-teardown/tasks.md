> 2026-11-01 整理：**superseded/history-only**。现行 owner：现行输出/退休协议归 RendererHost；未验浮窗场景归 `aurora-renderer-platform-parity` P02-EDITOR。
> 剩余：旧 2.4 保留未验收转交；禁止把旧 direct Device::WaitIdle 帧边界协议同步回当前 spec。下方较早状态与验收记录保留为历史。

## 1. Implementation

- [x] 1.1 Route explicit and native floating close through one deferred request path that marks closed and detaches font/window subscriptions.
- [x] 1.2 Retire floating and preview resources outside frame recording after device idle; batch close notifications after erase.
- [x] 1.3 Preserve recording-frame pointers, skip newly closed surfaces, snapshot floating iteration, and defer reentrant Shutdown until submission/presentation completes.
- [x] 1.4 Use one scoped listener identity per native window, with complete shutdown/destruction cleanup and unchanged close/move/resize payload forwarding.
- [x] 1.5 Add a headless regression target covering multiple keys, independent detach, self-detach during broadcast, and target teardown.

## 2. Verification

- [x] 2.1 Build EditorRender and EditorRenderTest and run the new headless regressions (including ASan where available).
- [x] 2.2 Review frame-pointer retention, callback iteration, thread/reentrancy assumptions, module boundaries, and CPU-before-GPU teardown order.
- [x] 2.3 Run strict OpenSpec validation and whitespace checks.
- [ ] 2.4 Transferred, not accepted: `aurora-renderer-platform-parity` P02-EDITOR owns floating/preview close-reopen, in-flight work, other windows continuing, recovery and shutdown validation under the current owned viewport-retirement protocol.

## Verification record (2026-10-16)

- The coordinating build compiled EditorRender and EditorRenderTest. EditorRenderTest passed all 6 CPU regressions in the normal build and all 6 under AddressSanitizer: 3 window-subscription lifetime tests and 3 frame-submission/fence tests from the companion skipped-frame repair. These are the same 6 tests, not 12 distinct scenarios.
- Static review confirms that explicit/OS close no longer erases resources before the next frame fence wait, and that frame-held surface pointers remain owned through submit/present.
- Strict OpenSpec validation and scoped `git diff --check` passed.
- Window-event dispatch and renderer APIs remain serialized on the editor thread. A GUI callback may request close/Shutdown, but may not destroy the renderer object itself while Tick is executing.
- The initial bounded Metal editor smoke failed with SIGBUS (exit -10), so it is not a native/GPU pass. Read-only crash analysis identified a stale Bullet module using the pre-RegisterMeta IModule vtable: the host's PostTick call reads the next RTTI object instead of a virtual function. See design.md for the binary/source evidence; this failed run remains part of the record.
- The coordinating full `cmake --build build --parallel 4` then succeeded (263 build steps), rebuilding the runtime modules. The repeated `./output/bin/SandboxEditor --console --frames 4 --rhi Metal` exited 0 and the old Bullet PostTick crash disappeared. However, its log reports `MTLCreateSystemDefaultDevice failed` and `aurora device init failed`; the SandboxModule renderer was not established. This verifies module startup/exit only, not GPU rendering or teardown. Historical duplicate Cocoa-class warnings remain separate architecture follow-up work. Log: `/private/tmp/sky-ui-native-smoke-rebuilt.log`.
- Real floating-window close/reopen/in-flight resource validation and Win32/VS2022 verification remain pending. Implementation and CPU-test completion are separate from T02 acceptance; task 2.4 remains open after the module startup/exit smoke.
- No commit or OpenSpec archive was performed.

## 2026-11-01 历史协议退役

当前 `EditorRendererWindows.cpp` 经 `RendererHost` 提交 owned `FramePacket`，由 `EditorViewportState` 发出 DestroyViewport 并等待精确 `ViewportDestroyed` 确认；editor 不再直接驱动 DeviceFrameContext fence、acquire semaphore 或 combined native submission。此 change 保留历史修复/失败证据，申请 history-only 归档，不把旧实现约束同步回主 spec。未执行的真实 editor 恢复/浮窗场景由 P02-EDITOR 明确接收。
