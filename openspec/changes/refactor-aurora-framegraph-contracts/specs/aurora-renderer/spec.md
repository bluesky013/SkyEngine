## ADDED Requirements

### Requirement: Explicit per-frame output export requests

FrameRenderTarget SHALL track exportRequested separately from persistent output initialization. Only successful Export of its exact image handle SHALL mark that per-frame request. Raster/copy/resolve declaration alone SHALL NOT export an output. Repeated image exports SHALL be idempotent without consuming additional budget; sealed builders SHALL still reject mutation. Scene, UI and TextureToScreen output producers SHALL explicitly export their completed outputs. Acquisition and NeedsOutput SHALL remain pre-build operations, and persistent offscreen initialization SHALL advance only on SubmitAccepted.

#### Scenario: Copy or resolve produces an output
- **WHEN** a feature copies or resolves into an output and explicitly exports it
- **THEN** the output survives graph culling and participates in submission without requiring an unrelated raster declaration

#### Scenario: Output was acquired but never exported
- **WHEN** no feature successfully exports an acquired output
- **THEN** it is abandoned through the existing lifecycle instead of being marked initialized by an incidental raster call

#### Scenario: Undefined exported content
- **WHEN** a feature exports an image whose final required content is undefined
- **THEN** graph analysis rejects the request; exportRequested does not certify content validity or GPU completion

### Requirement: Selected-technique resource readiness

Draw resolution SHALL check resource readiness against the requested queue Tag and exact selected variant. Missing techniques and unavailable active image backing SHALL skip only that queue's draw. Inactive parameters SHALL NOT impose residency. Invalid exact variants, device/epoch/type, views/ranges, LOD or skinning contracts SHALL remain explicit errors. Frame-local lookup indices SHALL be built only after prepared resource pins are updated and SHALL NOT become cross-frame caches.

#### Scenario: Unrelated technique has a missing texture
- **WHEN** an unused technique references an unavailable image and the requested technique has all active resources ready
- **THEN** the requested draw remains eligible

#### Scenario: Selected active texture is unavailable
- **WHEN** the selected variant requires an image with no usable pin/backing
- **THEN** only the corresponding queue draw is skipped without substituting another variant or resource version

#### Scenario: Selected input violates its contract
- **WHEN** a selected variant is missing or a ready binding has a wrong owner, epoch, type or range
- **THEN** resolution reports an explicit error instead of treating it as ordinary residency delay
