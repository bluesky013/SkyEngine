## ADDED Requirements

### Requirement: Custom pipelines consume foundation binding mechanisms

Aurora SHALL expose generic Pass binding validation through existing foundation interfaces/implementation targets without requiring consumers to link concrete built-in pipeline implementations. User plugin Pass implementations and declarations SHALL remain owned by their plugin. Generic binding SHALL preserve explicit static/dynamic policy, actual shader reflection validation and atomic draw-binding replacement.

#### Scenario: Standalone custom pipeline target
- **WHEN** a user pipeline target consumes shared declarations, Pass bindings and FrameGraph feature interfaces
- **THEN** it can build without linking the built-in Pipeline library or shader compiler to obtain C++ types, and no engine target depends on that user plugin

#### Scenario: Custom layout mismatch
- **WHEN** a user Pass provides a declaration or resource value incompatible with its actual shader reflection
- **THEN** binding reports an explicit error before modifying the draw or executing it on the GPU

### Requirement: Host explicitly assembles pipeline owners

The host SHALL explicitly assemble selected built-in or user pipeline features using existing RegisterRenderFeature/IFrameGraphFeature commands and actual module leases. Built-in Scene registration SHALL NOT be a prerequisite or implicit fallback for a custom pipeline. Pipeline plugins SHALL share the existing host packet producer, Renderer/Device, graph composition and retirement protocols rather than introduce independent submission or event-consumption loops.

#### Scenario: User pipeline replaces the built-in scene path
- **WHEN** a host selects a user pipeline and does not register the built-in Scene feature
- **THEN** the custom features produce the requested output without constructing built-in Scene passes or enforcing their layouts

#### Scenario: Owner retirement with outstanding work
- **WHEN** the host retires a plugin pipeline with accepted compilation, frame or GPU/presentation borrowers
- **THEN** new work stops for that owner and existing module/source/resource leases remain valid until their respective users have completed or drained
