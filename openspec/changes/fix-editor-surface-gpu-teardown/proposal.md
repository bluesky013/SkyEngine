## Why

Floating and preview windows currently release their swapchains and UI resources before the next frame's fence wait. Explicit floating-surface destruction also releases resources immediately, including when invoked from a paint callback whose frame still references them. A related window-event ownership bug leaves subscriptions behind because one renderer is connected under several window keys while keyed `DisConnect` removes only one entry.

## What Changes

- Queue floating-surface closure, stop new recording, and detach CPU font/window subscriptions before releasing GPU resources.
- Retire closed floating and preview surfaces at frame boundaries after `Device::WaitIdle`, covering submitted graphics work and queued presentation.
- Preserve surfaces already referenced by a recording frame through submission/presentation. Defer reentrant `Shutdown` until that frame finishes and prevent additional surface/preview recording.
- Give each native window its own scoped event-forwarding subscription, so closing one window and shutting down the owner remove the correct entries.
- Add headless event-lifetime regressions and explicitly retain real-GPU validation as an open acceptance task.

## Capabilities

### Modified Capabilities

- `editor-render`: frame-safe surface retirement and owned per-window event subscriptions.

## Impact

- EditorRender implementation/header, a private window-event helper, and a headless `EditorRenderTest` target.
- Builds on T01's paired font-atlas registration/removal; no new RHI API, RDG migration, multi-frame renderer, Core event API change, or interface layering change.
- A close may stall the editor thread while the device becomes idle. The current renderer uses one in-flight frame; asynchronous retirement can be considered after resource/frame ownership is designed.
