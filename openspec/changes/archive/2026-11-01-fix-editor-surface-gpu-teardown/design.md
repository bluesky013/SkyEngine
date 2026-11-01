## Context

`EditorRenderer` combines main, preview, and floating command buffers in one submission. The floating UI renderer and swapchain resources are owned by a `FloatingSurface`; the previous `activeSurfaces` list held raw pointers through submit and present. The companion skipped-frame repair now gives the frame-submission scope ownership of acquired viewport references through that same boundary. The previous Tick erased closed surfaces before `BeginFrame` waited for the previous submission. `DestroySurface` could erase one even while the current frame referenced it. Preview closure had the same early-release problem. Global Shutdown waited for the GPU, but did not guard a call made from `guiSource` during command recording.

## Decisions

### Close requests stop future use and preserve the current frame

`DestroySurface` and native close route through `RequestSurfaceClose`. It marks the surface closed, removes its font registry registration, and disconnects its per-window event binding. It does not release GPU resources or erase the owned object. Native closure remembers that the shell must be notified; explicit destruction does not request re-docking.

`RenderSurface` skips a closed surface. A surface whose recording already began may finish that frame; its object and resources remain alive through submit and present. Recording iterates a snapshot of surface pointers, so a GUI callback can create another surface without invalidating the vector iterator, and the new surface begins participating on a following frame if it was created after the snapshot.

### Retire outside active recording after the device is idle

Tick processes pending retirement before starting a frame and again after it finishes. Retirement waits for the device, then drops preview swapchain/window resources and shuts down/erases closed floating surfaces. `WaitIdle` is used rather than only the graphics frame fence because swapchain ownership also includes presentation. Closing multiple surfaces in the same batch requires one idle wait.

Re-docking notifications run after all erases, using copied panel identities and a callback snapshot. A notification can create/queue-close other surfaces without invalidating the retirement loop. Further queued closures are processed at the next boundary. Global shutdown suppresses these panel notifications.

Global Shutdown first stops new surface creation and detaches floating/font/window subscriptions. If called inside a frame, resource release is deferred until Tick has submitted/presented and ended that frame. Otherwise it waits for idle and releases all floating, main, preview, and content resources. Destruction calls this idempotent shutdown path as a fallback for partial initialization/omitted explicit shutdown.

### Each window owns one listener identity

The keyed `Event<IWindowEvent>` API removes the first registration matching a listener. Registering the same `EditorRenderer *` under several window keys cannot be accurately undone with one `DisConnect(this)` call.

The private `WindowEventBinding` instead subscribes its own address under exactly one window key and forwards close/move/resize payloads to the renderer. Each surface/preview owns its binding and destroys it while the renderer is alive, before GPU waits and native-window destruction. This avoids broad changes to the Core event contract. A close callback may reset its own binding; event dispatch has already taken a listener snapshot, and the forwarding method does not access members after invoking its target.

## Thread and reentrancy contract

- Window events, Tick, surface APIs, and Shutdown are serialized on the editor thread, as in the current host. This change does not make them thread-safe.
- Surface close or Shutdown requested from `guiSource` is supported; Tick reentry while a frame is active is ignored. The already-recording frame completes before resource destruction.
- The renderer object itself and its GUI callback target must outlive an executing Tick/callback. Destroying the owner from inside its own callback is not supported.
- A native close that occurs before a new surface/preview acquire prevents that acquire. A surface already being recorded retains its objects through the current frame; platform window/swapchain behavior when OS destruction happens during that recording still requires real backend validation.
- Current preview/floating window keys each have one production window-event proxy. Core event dispatch snapshots raw listener pointers: adding another listener under the same key later requires preserving any proxy that an earlier listener may remove until that broadcast has finished. Self-detachment is covered; arbitrary destruction of another pending listener during the same broadcast is not a new guarantee of this helper.

## Validation and acceptance

Headless tests verify independent subscriptions under multiple window keys, unchanged close/move/resize identity, self-detachment during a close broadcast, and no stale callbacks after all bindings/target are destroyed. These tests cover the CPU ownership bug; they do not emulate GPU completion.

T02 implementation can be reviewed/built independently, but T02 acceptance remains pending until native GPU validation covers closing/reopening several floating windows, closing preview, closing while work is submitted, main rendering afterward, and global shutdown on supported backends. A clean headless test run is not evidence that the GPU lifetime task is complete.

### Initial native smoke failure (2026-10-16)

The first bounded Metal editor smoke exited with SIGBUS (exit -10). Its faulting main-thread stack is `ModuleManager::Tick +281` calling RTTI data; it does not contain EditorRenderer or the frame-submission scope. Binary/source inspection establishes a stale loaded module ABI:

- The current host's PostTick dispatch uses `call *0x40(%rax)`. IModule gained a RegisterMeta virtual slot in committed change `952dae74` on 2026-10-15.
- The loaded Bullet dylib matches the disk image built on 2026-10-11 and has the old eight-entry virtual-function region. Its vtable symbol is at `0xdd298`; the crash's vptr is `0xdd2a8` (vtable +16).
- The host's `vptr +0x40` therefore reads `0xdd2e8`, which the image symbol table identifies as `typeinfo for sky::IModule`. Its typeinfo-vtable pointer matches the non-executable `__class_type_info` address at the fault.

This is a confirmed mixed-ABI module load, not a generic environment diagnosis and not evidence of GPU correctness. Cocoa duplicate-class warnings also appeared and remain separate observations; they do not replace the confirmed fault chain above.

### Rebuilt module startup/exit smoke (2026-10-16)

The coordinating full `cmake --build build --parallel 4` completed successfully (263 steps) and rebuilt the configured runtime modules. Repeating `./output/bin/SandboxEditor --console --frames 4 --rhi Metal` exited 0 and the old Bullet PostTick ABI crash disappeared. The rerun log nevertheless explicitly reports `MTLCreateSystemDefaultDevice failed` and `aurora device init failed`, so the SandboxModule renderer was not established. This is successful module startup/exit verification only; it exercises no renderer frame scope, GPU submission, presentation, or surface-resource retirement. Historical duplicate SkyCocoaView/SkyCocoaWindowDelegate warnings remain a separate architecture follow-up.

The original crash and the limited rerun result are both retained. Native floating-window close/reopen/in-flight teardown and minimize/recovery acceptance remain pending; exit 0 with failed renderer initialization cannot satisfy that gate. Rerun log: `/private/tmp/sky-ui-native-smoke-rebuilt.log`.
