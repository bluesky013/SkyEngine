# Delta: native-platform-backend

## MODIFIED Requirements

### Requirement: Native handle for the RHI
`NativeWindow::GetNativeHandle()` SHALL return the platform handle used by the RHI to create its surface
(`HWND` on Windows, the window's `CAMetalLayer` on macOS — the Metal swapchain and the MoltenVK
`VK_EXT_metal_surface` path both consume the layer directly).

#### Scenario: Surface from handle
- **WHEN** the RHI creates a swapchain for a window
- **THEN** it SHALL use the handle from `GetNativeHandle()` unchanged

#### Scenario: macOS layer handle
- **WHEN** a `NativeWindow` is created on macOS
- **THEN** its content view SHALL be backed by a `CAMetalLayer` and `GetNativeHandle()` SHALL return that layer
