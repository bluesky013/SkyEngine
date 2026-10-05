## Context

The editor runs a single-threaded frame loop: `Application::Loop` pumps events (`PoolEvent`) then
ticks modules (rendering). Two gaps combined to hang the process: the Win32 window proc never handled
`WM_CLOSE`/`WM_DESTROY`, and the render loop kept acquiring/presenting a surface whose window was
closing. Vulkan's `vkAcquireNextImageKHR` with `UINT64_MAX` blocks indefinitely when no image becomes
available, so a closing surface wedges the main thread.

## Goals / Non-Goals

**Goals:**

- Window close is an event owners can react to; the frame loop stops presenting a closing window.
- Closing the main window exits; closing a secondary window does not.
- The frame loops never acquire an image they cannot present, and never leave one unreleased.

**Non-Goals:**

- A finite swapchain-acquire timeout. `vkAcquireNextImageKHR`'s `timeout` is a soft bound
  (the call may return early on out-of-date) and is **Vulkan-only** — DX12's
  `GetCurrentBackBufferIndex` has no timeout. It also cannot fix the root cause (presenting a window
  that is going away). The acquire keeps `UINT64_MAX`.
- Recreating a lost surface/swapchain; a `LOST` swapchain currently stops presenting.
- Bounding the DX12 present wait (`D3D12SwapChain::Present`) or the fence wait — those are genuine
  wait-for-GPU operations.
- `ColorPicker`'s wheel texture is app-lifetime (its widget is a singleton in
  `ReflectedWidgetRegistry`), not a per-rebuild leak.

## Decisions

- **Event-driven close.** Add `IWindowEvent::OnWindowClose(const NativeWindow*)`, broadcast from
  `WM_CLOSE` before the window is destroyed. Owners (the renderer) react by dropping the window's
  viewport, so acquire is never attempted on a closing window. This is backend-agnostic and matches
  the spec's "if forward progress cannot be guaranteed, don't block on acquire" guidance.
- **Main vs secondary window.** `Win32Window::IsMainWindow()` compares against the platform's
  first-created main window (first-wins `SetMainWindow`); `WM_CLOSE` posts `WM_QUIT` only for it.
  `DefWindowProc` still destroys the window.
- **Quit stops the frame first.** `Application::Loop` returns right after event pumping when `exit`
  is set, so the loop never ticks/renders after a quit was posted (the main HWND may already be
  destroyed).
- **Drop a closed secondary window outside its message handler.** `EditorRenderer` sets a flag in
  `OnWindowClose` and, on the next `Tick`, resets the preview viewport and window — never destroying
  the `NativeWindow` object from inside its own window proc.
- **Check extent before acquire; release unusable images.** In both `EditorRenderer::Tick` and
  `AuroraModule::Tick`, the surface extent is read (valid after `Begin()`) before `Acquire()`; a zero
  extent skips the frame without acquiring, and an acquired-but-unusable image is `Release()`d to
  keep the queue moving.
- **Surface lost maps to `LOST`.** `VulkanSwapChain::AcquireNextImage` maps
  `VK_ERROR_SURFACE_LOST_KHR` / `VK_ERROR_DEVICE_LOST` to `SwapChainStatus::LOST` so `Begin()`
  short-circuits.
- **Release the demo icon texture on panel destruction.** `UIRenderer::Shutdown()` clears its texture
  map while the device is valid, so the later erase is a no-op.

## Risks / Trade-offs

- [A `LOST` swapchain stops presenting rather than being rebuilt] -> documented non-goal; recreating
  the surface/swapchain is a follow-up.
- [Could not deterministically reproduce the exact interaction] -> the fix targets the mechanism
  (unhandled close + presenting a closing window); verified by closing the preview window (stays
  responsive) and the main window (exits).
- [`Application::Loop` change affects all apps] -> it only skips a frame after quit was requested,
  which is strictly correct.

## Migration Plan

Additive/behavioral, no API break: `IWindowEvent` gains a virtual with a default no-op; the acquire
API is unchanged. Rollback is reverting the touched files.
