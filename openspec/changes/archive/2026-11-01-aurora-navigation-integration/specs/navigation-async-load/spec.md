## REMOVED Requirements

### Requirement: Asynchronous tile prefetch

**Reason**: The current manifest already owns resident bytes; the old task only copied them and performed no IO. Budgeted owned-manifest paging removes unnecessary task lifetime hazards.
**Migration**: Use main-thread coordinate paging over the runtime-owned layered manifest. A future external IO source must provide a separate explicit completion contract.

### Requirement: Polled completion and cancellation

**Reason**: In-memory manifest paging has no asynchronous prefetch operation to poll or cancel.
**Migration**: Out-of-range coordinates are skipped before installation; manifest replacement discards old resident payloads safely. Asynchronous path query completion/cancellation remains supported.

### Requirement: Per-frame load budget

**Reason**: Budget semantics move to the streaming coordinate contract so all layers of a coordinate are handled together.
**Migration**: Set the coordinate budget through the navigation streaming interface.

### Requirement: Failure tolerance

**Reason**: Paging failures now belong to the owned layered manifest and explicit backend tile installation contracts.
**Migration**: Missing, invalid or stale payloads must fail installation without crashing or being counted as loaded.

## ADDED Requirements

### Requirement: Budgeted resident-manifest paging

Streaming SHALL apply runtime-owned resident tile payloads on the world tick within a coordinate budget. It SHALL NOT start an asynchronous task merely to copy already resident bytes. Real asynchronous path queries SHALL retain their separate completion and cancellation contracts.

#### Scenario: Resident coordinate enters range

- **WHEN** a manifest coordinate enters the load radius
- **THEN** paging SHALL apply its layers on the main thread within the configured coordinate budget

#### Scenario: Pending coordinate leaves range

- **WHEN** an unapplied coordinate leaves the load radius
- **THEN** paging SHALL skip it without scheduling stale work
