## Why

`DeviceFrameContext::BeginFrame` waits for and resets the current frame fence before the editor checks viewport availability. A minimized/lost surface or a failed acquire then returns without a submission signaling that fence. On fence implementations where Reset advances or clears the waited state, a later frame can wait indefinitely. A successful acquire with no usable backbuffer also previously called Present without a submission signaling its render-done semaphore.

## What Changes

- Add an EditorRender-private frame scope that pairs every BeginFrame with one fence-bearing submission, including skipped frames, and one EndFrame.
- Register every successful main/preview/floating acquire with that scope; consume acquire semaphores, signal render-done semaphores, and release/present acquired images after submission even if no draw commands can be recorded.
- Preserve the frame wait before viewport Begin, so swapchain maintenance cannot destroy resources before prior frame work completes.
- Use a conservative TOP-stage acquire wait for all window work, including preview transfer writes; retain BOTTOM-stage render-done signals and a single combined submission.
- Add headless mocks using the real DeviceFrameContext frame/fence logic to test skip/recovery, unusable acquired images, and combined multi-window submission.

## Capabilities

### Modified Capabilities

- `editor-render`: completion of skipped frames and balanced acquired-image synchronization.

## Impact

- EditorRenderer, a private frame-submission helper, and EditorRenderTest.
- No engine/aurora implementation/interface change, backend API change, RDG migration, or multi-frame scheduling change.
- Builds on `fix-editor-surface-gpu-teardown`: resources remain owned through the frame scope's submission/presentation, then close retirement can wait and release them.
- Native GPU minimized/restore/acquire-failure testing remains open; CPU synchronization tests do not establish platform presentation correctness.
