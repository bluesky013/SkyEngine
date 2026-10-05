## Why

Closing windows could leave the process stuck. Two gaps combined: the Win32 message path never
handled `WM_CLOSE` (so closing any window never posted `WM_QUIT`), and the frame loop kept
acquiring/presenting a surface whose window was closing. A Vulkan-only finite acquire timeout is
**not** the right tool: `vkAcquireNextImageKHR`'s `timeout` is a soft bound and Vulkan-specific
(DX12 has no equivalent), and the real requirement is to **not acquire/present a window that is
going away**. The fix is event-driven: map window close to an engine event and stop presenting.

## What Changes

- Map a window close to a new `IWindowEvent::OnWindowClose(window)` broadcast **before** the window
  is destroyed; closing the **main** window requests application exit, closing a **secondary**
  window (e.g. the standalone preview) does not.
- Stop the frame **before** ticking/rendering when a quit is requested, so no frame renders against
  a window that is being destroyed.
- Harden the editor and launcher frame loops: check the surface extent **before** acquiring (a
  zero-size surface must not acquire an image it cannot present) and release an
  acquired-but-unusable image; the editor drops a closed preview window's viewport in response to
  `OnWindowClose` so it is never acquired again.
- Map `VK_ERROR_SURFACE_LOST_KHR` / `VK_ERROR_DEVICE_LOST` to the swapchain `LOST` status.
- Release the demo icon texture when its panel is destroyed, so repeated shell rebuilds do not leak
  GPU images.

Explicitly **not** changed: the swapchain acquire keeps `UINT64_MAX` (no timeout) — the fix is
event/state-driven, not a timeout.

## Capabilities

### New Capabilities

<!-- none -->

### Modified Capabilities

- `native-platform-backend`: window close is mapped via `IWindowEvent::OnWindowClose`; main-window close exits, secondary windows close independently.
- `editor-application`: a quit request stops the frame before ticking/rendering.
- `editor-render`: extent is checked before acquiring; an acquired-but-unusable image is released; a closed preview window's viewport is dropped and never acquired.
- `aurora-rhi-core`: swapchain acquire reports surface/device loss as `LOST`.
- `aurora-launcher`: the launcher frame loop checks extent before acquiring and never leaves an acquired image unreleased.

## Impact

- `engine/framework`: `IWindowEvent::OnWindowClose`; `Win32Window` broadcasts it on `WM_CLOSE` (+ `IsMainWindow`); `Application::Loop` skips the frame when quit is requested.
- `engine/aurora/rhi`: `VulkanSwapChain` surface/device-lost mapping.
- `engine/aurora/adaptor`: `AuroraModule::Tick` acquire ordering/release.
- `engine/sandbox/render`: `EditorRenderer` subscribes to `IWindowEvent` and drops the preview viewport on close; frame-loop acquire ordering/release.
- `engine/sandbox/shell`: `ReflectionDemoPanel` releases its icon texture on destruction.
