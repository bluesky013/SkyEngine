## ADDED Requirements

### Requirement: Metal participates in the explicit frame-context protocol

Metal `DeviceFrameContext` instances SHALL support `TryBeginSlot`, fence/pool reset, typed lease validation and `TryCollect` through the common `BackendFrameContext` protocol. The common layer SHALL NOT reject Metal solely because its backend is Metal; unsupported capability SHALL be reported by the Metal implementation or by a specific unsupported operation.

#### Scenario: Metal slot begin
- **WHEN** a Metal frame context with a valid device epoch calls `TryBeginSlot`
- **THEN** it SHALL return a `DeviceFrameLease` or an explicit `FrameBusy`/`FrameError`, not an unconditional `UNSUPPORTED`

#### Scenario: Metal fence and pool lifecycle
- **WHEN** a Metal submission completes and its slot is reused
- **THEN** the fence SHALL report `SIGNALED`, `TryReset` SHALL advance it, and the command pool SHALL be reusable

### Requirement: Metal offscreen clear/copy/readback

The Metal frame context SHALL compile and record an offscreen graph containing single-color raster passes with draws, optional D32 depth, RGBA8 MSAA resolve and selected mip/layer attachment views, plus buffer/image copy passes; it SHALL submit through the common slot protocol, poll completion, and expose readback contents.

#### Scenario: Clear and read back
- **WHEN** a graph clears an RGBA8 render-target image and copies it to a host-readable buffer
- **THEN** the submitted frame completes and the readback contains the clear color

#### Scenario: Fullscreen draw
- **WHEN** a raster pass contains a valid draw, pipeline and graph bindings
- **THEN** the compiled Metal plan SHALL encode the draw into the render pass and the readback SHALL contain the shader output

#### Scenario: MSAA resolve
- **WHEN** a raster pass renders to an RGBA8 multisample image and declares a resolve target
- **THEN** the resolved target SHALL contain the rendered/cleared color

#### Scenario: Depth attachment
- **WHEN** a raster pass declares a D32 depth image
- **THEN** the encoded render pass SHALL apply the requested depth load/store/clear

#### Scenario: Selected mip/layer
- **WHEN** a raster pass selects a non-zero mip/layer view
- **THEN** only that cell SHALL be rendered and copied by the requested readback

#### Scenario: Two slots in flight
- **WHEN** two frames are submitted before completion is polled
- **THEN** both slots SHALL complete independently and their readbacks SHALL match

#### Scenario: Explicit unsupported subset
- **WHEN** the graph contains compute, custom passes, or an unsupported attachment/view form
- **THEN** compile SHALL return an explicit `FrameError` with an unsupported/invalid reason, not silently record an empty frame

### Requirement: Metal transient resources use the shared frame pool

The Metal device SHALL provide the same `FrameTransientPool` used by Vulkan and DX12 so transient graph buffers/images are allocated and retired with the frame lease.

#### Scenario: Transient image allocation
- **WHEN** a Metal graph declares a transient render-target image
- **THEN** it SHALL be acquired through the device transient pool and remain pinned until the slot retires

#### Scenario: Imported resource validation
- **WHEN** an imported buffer/image belongs to another device or has an incompatible descriptor
- **THEN** compile SHALL return `INVALID_ARGUMENT` without consuming the lease
