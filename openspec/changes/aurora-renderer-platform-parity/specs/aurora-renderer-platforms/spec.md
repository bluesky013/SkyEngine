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

The legacy path SHALL be removed only after the corresponding platform passes the native capability gate with a real window or supported offscreen output, including drawable timeout, resize/close, and failure-exit behavior.

#### Scenario: Capability gate blocks premature removal
- **WHEN** a platform has not passed the native host gate
- **THEN** the legacy path SHALL remain and the unification task SHALL stay blocked

#### Scenario: macOS evidence
- **WHEN** the macOS/Metal host is claimed complete
- **THEN** records SHALL include real-window and offscreen host runs, not only a successful compile or Windows results

#### Scenario: Non-Windows tests
- **WHEN** the unified path is enabled on a non-Windows host
- **THEN** host lifecycle tests SHALL cover init, submit, drain, shutdown, resize/close, and failure paths on that host
