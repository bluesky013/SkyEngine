# navigation-async-load Specification

## Purpose
TBD - created by archiving change navigation-async-load. Update Purpose after archive.
## Requirements
### Requirement: Budgeted resident-manifest paging

Streaming SHALL apply runtime-owned resident tile payloads on the world tick within a coordinate budget. It SHALL NOT start an asynchronous task merely to copy already resident bytes. Real asynchronous path queries SHALL retain their separate completion and cancellation contracts.

#### Scenario: Resident coordinate enters range

- **WHEN** a manifest coordinate enters the load radius
- **THEN** paging SHALL apply its layers on the main thread within the configured coordinate budget

#### Scenario: Pending coordinate leaves range

- **WHEN** an unapplied coordinate leaves the load radius
- **THEN** paging SHALL skip it without scheduling stale work

