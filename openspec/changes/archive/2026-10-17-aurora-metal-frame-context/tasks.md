## 1. Sync and transient primitives

- [x] 1.1 Add `MetalFence::Query` and `MetalFence::TryReset` using the shared-event value.
- [x] 1.2 Override `MetalCommandPool::TryReset`.
- [x] 1.3 Add `FrameTransientPool` ownership and a `GetTransientPool()` accessor to `MetalDevice`.

## 2. Explicit slot protocol

- [x] 2.1 Remove the unconditional `API::METAL` rejection from `TryBeginSlot`.
- [x] 2.2 Remove the `API::METAL` rejection from `CanChangeTracking`, `TrackResource`, `GetResourceState` and `UntrackResource`.
- [x] 2.3 Keep unsupported Metal operations explicit in the Metal compile path.

## 3. Metal frame compiler

- [x] 3.1 Implement a Metal graph-resource resolver using the shared transient pool and device ownership checks.
- [x] 3.2 Implement compile for `BufferCopyPass` and `BufferImageCopyPass` into blit steps.
- [x] 3.3 Implement compile for a single-color `FrameRasterPass` with draws, mapping load/store/clear into `RenderingInfo`.
- [x] 3.4 Add pipeline/resource-group/vertex-index binding for draws; return explicit `UNSUPPORTED` for compute, custom passes and unsupported views.
- [x] 3.5 Implement dispatch/collect by recording the steps into one command buffer and publishing the recording.
- [x] 3.6 Add D32 depth attachment load/store/clear and pipeline depth format.
- [x] 3.7 Add RGBA8 MSAA color resolve through the Metal blit/resolve helper.
- [x] 3.8 Add selected mip/layer attachment and sampled-image view creation.
- [x] 3.9 Add D32 MSAA depth resolve through the Metal depth store-action resolve pass.

## 4. Offscreen test

- [x] 4.1 Add a Metal-only test that creates an RGBA8 render-target image and a host-readable readback buffer.
- [x] 4.2 Build a frozen graph that clears the image and copies it to the buffer.
- [x] 4.3 Begin a slot, compile, dispatch, collect, submit, poll completion and verify the clear pixels.
- [x] 4.4 Add an explicit-unsupported case (compute) and a fullscreen-draw readback case.
- [x] 4.5 Add MSAA resolve, depth clear, two-slot in-flight and selected mip/layer readback cases.
- [x] 4.6 Add a depth-resolve case alongside color resolve and readback.

## 5. Verify

- [x] 5.1 Build `Aurora.Metal` and the new test target on macOS.
- [x] 5.2 Run `Aurora.RHITest` and the full suite; record existing unrelated failures.
- [x] 5.3 Run `openspec validate aurora-metal-frame-context --strict --no-interactive` and `git diff --check`.
- [x] 5.4 Record verification results in `tasks.md`.

## Verification record

- Metal now participates in the common slot/lease/fence protocol; `MetalFence` implements `Query`/`TryReset`, `MetalCommandPool` implements `TryReset`, and `MetalDevice` owns a shared `FrameTransientPool`.
- `MetalDeviceFrameContext` compiles clear/load/store raster passes, fullscreen draws with `GraphicsPipeline`/`ResourceGroup`, buffer copies and buffer/image copies; it records and submits them through the common protocol and publishes completion.
- Added `MetalFrameContextTest` cases:
  - clear + image-to-buffer readback;
  - fullscreen triangle through an MSL shader with green readback;
  - compute placeholder rejection with `UNSUPPORTED`;

- `Aurora.RHITest`: 142/142 passed; Metal frame-context cases cover clear/copy, fullscreen draw, MSAA resolve, depth clear + depth resolve, two in-flight slots, 32-frame slot reuse, selected mip/layer, sampled-image view and upload staging stress.
- Full `ctest`: 49/49 passed on the final run (an earlier `AssetManagerTest.ConcurrentLoadStress` failure did not reproduce).
- `Aurora.Renderer.ModuleTest` (including the Cocoa native host case) and `Aurora.Renderer.StaticModuleTest` now pass on macOS (previously failing).
- `AuroraModule` was unified onto `RendererHost` and the non-Windows legacy host path was removed; the native Cocoa window test completes frames and drains on shutdown.
- Remaining: compute/custom implementation (placeholders still return `UNSUPPORTED`), iOS/Android device validation.
