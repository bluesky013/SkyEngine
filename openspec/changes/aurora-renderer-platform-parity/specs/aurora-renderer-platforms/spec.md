## ADDED Requirements

### Requirement: All Aurora hosts use one RendererHost path

After the platform capability gate passes, `AuroraModule` SHALL use `RendererHost` and `IRendererProvider` on every supported platform. It SHALL NOT retain a platform-specific legacy host loop, a `legacy` flag, or direct ownership of `Device`, `DeviceFrameContext`, `CommandPool`, `CommandBuffer`, or `ClientViewport` for non-Windows hosts.

#### Scenario: Non-Windows module init
- **WHEN** the Aurora module initializes on macOS, Linux, or another non-Windows host that has passed the capability gate
- **THEN** it SHALL create and drive the renderer through `RendererHost` like the Windows host, not through `InitLegacy`

#### Scenario: Legacy host code is absent
- **WHEN** `engine/aurora/adaptor` is inspected after the unification
- **THEN** `InitLegacy`, `StartLegacy`, `TickLegacy`, the `legacy` flag, and the guarded direct-device members SHALL NOT exist

#### Scenario: No hidden platform split
- **WHEN** the module lifecycle is reviewed
- **THEN** `Init`, `Start`, `Tick`, and `Shutdown` SHALL NOT branch on `_WIN32` to choose a different host protocol

### Requirement: Host unification requires platform evidence

The unified host SHALL expose explicit unsupported capability failures. A platform SHALL be claimed validated only after its native capability gate passes with a real window or supported offscreen output, including drawable timeout, resize/close, and failure-exit behavior.

#### Scenario: Capability gate blocks premature removal
- **WHEN** a platform has not passed the native host gate
- **THEN** native acceptance SHALL remain open and compilation or a bounded number of host ticks SHALL NOT substitute for the missing evidence

#### Scenario: macOS evidence
- **WHEN** the macOS/Metal host is claimed complete
- **THEN** records SHALL include real-window and offscreen host runs, not only a successful compile or Windows results

#### Scenario: Non-Windows tests
- **WHEN** the unified path is enabled on a non-Windows host
- **THEN** host lifecycle tests SHALL cover init, submit, drain, shutdown, resize/close, and failure paths on that host

### Requirement: Checked native outputs retain exact surface lifetimes

A checked output SHALL retain its exact native surface through acquisition, queued/GPU use and presentation retirement. Acquire SHALL remain bounded and distinguish busy, unsupported and lost surfaces; direct and checked protocols SHALL NOT be mixed on one swapchain.

#### Scenario: Metal drawable acquisition is pending
- **WHEN** a native drawable request has not completed
- **THEN** checked acquire SHALL report busy without blocking the renderer coordinator
- **AND** resize, close and teardown SHALL retain the layer and request until safe collection

#### Scenario: Mobile native surface is replaced
- **WHEN** Android replaces its ANativeWindow or iOS disconnects and reconnects a CAMetalLayer
- **THEN** the host SHALL retire the old viewport generation before creating an output for the newest surface
- **AND** it SHALL preserve scene state and continue control/resource progress while the surface is absent

#### Scenario: Presentation capability is unavailable
- **WHEN** an output requires an unsupported checked-presentation capability
- **THEN** the host SHALL expose an explicit unsupported error instead of repeatedly logging a black output

### Requirement: Metal resources respect native capabilities and view contents

Metal SHALL reject unsupported image formats, multisample array configurations and sampler modes before passing invalid descriptors to native APIs. Advertised image format features SHALL agree with creation support. Sampled subresource views SHALL expose the declared mip and layer contents and retain any backend copy resources until GPU retirement.

#### Scenario: Older iOS lacks an image or sampler capability
- **WHEN** the OS or GPU does not support a requested compressed format, multisample array or sampler address mode
- **THEN** resource creation SHALL fail explicitly without relying on a native validation assertion

#### Scenario: Restricted depth array view is sampled
- **WHEN** a raster pass samples a selected depth array mip and layer after an earlier pass writes it
- **THEN** it SHALL observe that selected content
- **AND** any backend copy workaround SHALL execute after the producer and before sampling, with resources retained through completion
