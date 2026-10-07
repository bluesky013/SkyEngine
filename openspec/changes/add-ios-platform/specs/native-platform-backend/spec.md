# Delta: native-platform-backend

## ADDED Requirements

### Requirement: iOS RHI surface contract
On iOS, `NativeWindow::GetNativeHandle()` SHALL return the window's `CAMetalLayer` (the same surface contract as
macOS), so the Metal swapchain and the MoltenVK `VK_EXT_metal_surface` path can create their surface unchanged.
The iOS window/event backend that provides this handle — and its frame loop, input mapping, DPI and lifecycle — is
specified by the `ios-uikit-backend` capability (change `add-ios-uikit-backend`).

#### Scenario: iOS surface handle
- **WHEN** a window is created on iOS
- **THEN** `GetNativeHandle()` SHALL return its `CAMetalLayer`

#### Scenario: RHI consumes the handle unchanged
- **WHEN** the RHI creates a Metal/Vulkan surface for the iOS window
- **THEN** it SHALL use the handle from `GetNativeHandle()` without platform-specific handling
