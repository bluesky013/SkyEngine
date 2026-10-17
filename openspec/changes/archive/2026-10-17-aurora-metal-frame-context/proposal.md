## Why

Metal is currently outside Aurora's new frame protocol: `MetalDeviceFrameContext` is an empty wrapper and the common `BackendFrameContext` explicitly returns `UNSUPPORTED` for Metal slot, tracking and compile operations. The only working macOS path is the legacy direct-device host in `AuroraModule`, which is blocked from removal by `aurora-renderer-platform-parity`.

This change starts the missing Metal frame-context implementation with a verifiable first milestone instead of treating the whole parity effort as one unit: an offscreen Metal context that can begin a slot, compile a minimal graph, record it, submit it, poll completion and read back pixels. Once this path passes, later milestones can add draws, compute, uploads, native-window output and finally remove the legacy host.

## What Changes

- Make the common `BackendFrameContext` slot/tracking protocol usable by Metal instead of hard-rejecting `API::METAL`.
- Add Metal fence reset/query and command-pool reset support required by the explicit slot protocol.
- Add a Metal `FrameTransientPool` to the Metal device.
- Implement `MetalDeviceFrameContext` compile/dispatch/collect for an initial offscreen subset:
  - buffer copies;
  - buffer-to-image and image-to-buffer copies;
  - single-color raster clear/load/store with draws, pipelines and resource groups;
  - D32 depth attachment clear/load/store;
  - RGBA8 MSAA color resolve and selected mip/layer attachment views;
  - explicit `UNSUPPORTED` for compute, custom passes and unsupported attachment/view forms until later milestones.
- Add macOS Metal offscreen tests for clear/copy/readback, a fullscreen triangle, MSAA resolve, depth clear, two in-flight slots and selected mip/layer rendering.
- Keep the legacy `AuroraModule` host path untouched and blocked; host unification remains in `aurora-renderer-platform-parity`.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `aurora-rhi-metal`: adds the explicit Metal frame-context protocol (slot begin, compile/dispatch/collect, fence/pool reset, offscreen clear/copy/readback).

## Impact

- `engine/aurora/rhi/common/FrameContext.cpp` (remove Metal hard rejection and use backend capabilities).
- `engine/aurora/rhi/metal` (`MetalDeviceFrameContext`, Metal graph resources/compiler, fence/pool/transient-pool support).
- `engine/aurora/rhi/test` (Metal offscreen frame-context test).
- No host removal, no public API break, no legacy path deletion.
- Draws, compute, uploads, native-window output and host unification remain follow-up milestones.
