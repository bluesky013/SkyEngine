## Context

The editor currently uses one in-flight frame. `DeviceFrameContext::BeginFrame` waits and resets its fence, while EndFrame only advances frame state and resets frame allocation. Early returns after viewport Begin failure, zero extent, Acquire failure, or a null backbuffer could therefore leave a reset fence without any signaling submission. The Vulkan reset explicitly clears the fence; DX12 Reset advances its pending value. Metal currently advances the waited value when Submit takes the next fence value, rather than in Reset, so the same skipped-frame hang is not established for the current Metal implementation.

The old acquired-but-unusable branch called `ClientViewport::Release`, which presents while waiting for its render-done binary semaphore. No submit consumed the successful acquire semaphore or signaled render-done. Floating surfaces had the same malformed-image branch; preview also needs to keep acquired-image ownership balanced when it cannot record.

## Backend source audit

- VulkanQueue always builds one `VkSubmitInfo2` and calls `vkQueueSubmit2` with the requested fence, including when its command-buffer count is zero.
- D3D12Queue skips ExecuteCommandLists when the list is empty, but still processes semaphore waits/signals and signals the supplied fence.
- MetalQueue creates a no-op native command buffer for an empty batch when signal semaphores or a fence are requested, then encodes fence/semaphore signals and commits it.
- ClientViewport Acquire stores the current acquired image and exposes its acquire/render-done semaphore pair; Release presents that image waiting on render-done. An unsuccessful Acquire must not be released, while a successful acquire must be paired even when GetBackbuffer is null.

This is a source audit, not a native GPU execution result. No backend or RHI interface changes are needed for the local fix.

## Decision: one frame scope owns submission and acquired images

The private `EditorFrameSubmission` constructor calls BeginFrame. It must be constructed before any viewport Begin, because Begin may rebuild an out-of-date swapchain. Work that needs the completed-frame safe point, such as retirement of old UI textures with one in-flight frame, can run immediately after construction and before viewport acquisition.

`Acquire(RenderViewport&)` first calls Begin and checks nonzero extent, then attempts Acquire. Only successful acquisitions are registered. Their synchronization pair and viewport pointer are retained even when GetBackbuffer is null; a null backbuffer returns false to the caller to suppress drawing, without dropping the acquired-image obligations.

Main, preview, and floating command buffers are added after recording completes. At scope exit, exactly one combined submission carries all completed command buffers, every acquired window's semaphore pair, and the current frame fence. The same operation submits a fence-only batch if the frame skipped before any acquisition. The scope then releases each acquired viewport and ends the frame. No early return can bypass completion after BeginFrame.

Acquire waits use TOP, covering both graphics and the preview's transfer work. Render-done signals retain BOTTOM. This is intentionally conservative for the current direct-encoder editor loop.

## Interaction with window closure

The frame scope holds viewport pointers through submission/presentation. T02's close path retains their owning surfaces while `frameActive` is true, so close/Shutdown requested from a GUI callback cannot invalidate those pointers. After the scope ends, Tick leaves the active frame and performs pending shutdown or device-idle surface retirement.

Acquired-but-unusable images can have their semaphore cycle balanced without an Image pointer. The editor cannot emit an image-layout transition when the backend supplies no Image; actual presentation validity and recovery for such a malformed backend result must still be checked on native backends. This fix removes the guaranteed missing-signal defect and preserves the existing Release contract; it does not claim full device-loss recovery.

## Validation

Three headless regressions use the real DeviceFrameContext BeginFrame/EndFrame implementation with a CPU fence/queue/viewport model. They cover lost Begin/zero extent/failed Acquire followed by a healthy frame without an incomplete fence wait; successful Acquire with no backbuffer and the required wait/signal/release cycle; and a combined main/preview/floating batch that retains synchronization for an unusable acquired floating image.

Native backend acceptance remains pending: repeated minimize/restore, out-of-date/lost/acquire-failure recovery, presentation after an unusable acquire where reproducible, and close/Shutdown with submitted work. Build/mock success must be reported separately from those GPU results.

The initial bounded Metal native smoke failed with SIGBUS in the host's PostTick virtual dispatch to an old Bullet IModule vtable. The current host includes the RegisterMeta slot added on 2026-10-15; the loaded 2026-10-11 Bullet image does not. Host disassembly and the crash vptr show PostTick reading the following `typeinfo for sky::IModule` object, then branching into its RTTI vtable. The full read-only evidence is recorded in the companion [surface teardown design](../fix-editor-surface-gpu-teardown/design.md#initial-native-smoke-failure-2026-10-16).

The subsequent full build (`cmake --build build --parallel 4`, 263 steps) succeeded and rebuilt the runtime modules. The repeated bounded Metal run exited 0 and the old Bullet ABI crash disappeared. Its log still reports `MTLCreateSystemDefaultDevice failed` / `aurora device init failed`, so the SandboxModule renderer was not established. The rerun verifies module startup/exit, not a native frame-scope/fence/presentation execution. Historical duplicate Cocoa-class warnings also remain a separate architecture follow-up. Both runs are retained in the verification record; the 6 passing CPU tests remain separate evidence, and native GPU acceptance stays pending. Rerun log: `/private/tmp/sky-ui-native-smoke-rebuilt.log`.
