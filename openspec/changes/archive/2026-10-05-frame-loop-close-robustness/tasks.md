## 1. Window close event (native-platform-backend)

- [x] 1.1 Add `IWindowEvent::OnWindowClose(const NativeWindow*)`; broadcast it from `WM_CLOSE` before the window is destroyed.
- [x] 1.2 Add `Win32Window::IsMainWindow()` and post `WM_QUIT` on main-window close; secondary windows are destroyed without exiting.

## 2. Quit stops the frame (editor-application)

- [x] 2.1 `Application::Loop` returns after event pumping when a quit is requested, so no frame ticks/renders against a closing window.

## 3. Swapchain acquire correctness (aurora-rhi-core)

- [x] 3.1 `VulkanSwapChain::AcquireNextImage` maps `VK_ERROR_SURFACE_LOST_KHR` / `VK_ERROR_DEVICE_LOST` to `SwapChainStatus::LOST`.
- [x] 3.2 Keep the acquire timeout at `UINT64_MAX` (no bounded-timeout workaround).

## 4. Editor frame loop (editor-render)

- [x] 4.1 Check the surface extent before `Acquire()` (zero extent skips the frame) and release an acquired-but-unusable image.
- [x] 4.2 `EditorRenderer` subscribes to `IWindowEvent`; on preview-window close it drops the preview viewport/window on the next `Tick` (never acquired again).
- [x] 4.3 Release the demo icon texture when its panel is destroyed.

## 5. Launcher frame loop (aurora-launcher)

- [x] 5.1 `AuroraModule::Tick` checks the surface extent before `Acquire()` and releases an acquired-but-unusable image (same acquire-then-return leak).

## 6. Validation

- [x] 6.1 Build `SandboxEditor` and `AuroraRender`; openspec `--strict` valid.
- [x] 6.2 Editor stays responsive after closing the standalone preview window; closing the main window exits (code 0).
