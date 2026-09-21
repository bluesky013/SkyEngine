# navigation-async-load Specification

## Purpose
TBD - created by archiving change navigation-async-load. Update Purpose after archive.
## Requirements
### Requirement: Asynchronous tile prefetch

Tile payload preparation SHALL run as a `Task`, not on the world tick, and the prepared payload SHALL be applied
on the main thread.

#### Scenario: Prefetch runs off the world tick

- **WHEN** a tile enters the load radius
- **THEN** a prefetch task SHALL be started instead of adding the tile synchronously

#### Scenario: Application is main-thread only

- **WHEN** a prefetch completes
- **THEN** the tile SHALL be added to the nav mesh only from the main thread during the tick

### Requirement: Polled completion and cancellation

The loader SHALL expose polled completion, and SHALL allow cancelling a pending tile.

#### Scenario: Completion is observable

- **WHEN** a prefetch finishes
- **THEN** it SHALL report finished and be eligible for application

#### Scenario: Out-of-range pending tile is cancelled

- **WHEN** a pending tile leaves the unload radius before it is applied
- **THEN** its task SHALL be cancelled and it SHALL not be applied

### Requirement: Per-frame load budget

Tile application SHALL be capped per frame.

#### Scenario: Budget limits applied tiles

- **WHEN** more prefetches are finished than the configured budget
- **THEN** only the budgeted number SHALL be applied this tick, the rest remaining pending

### Requirement: Failure tolerance

A failed or stale load SHALL NOT block gameplay or crash.

#### Scenario: Missing payload

- **WHEN** a prefetch has no source payload
- **THEN** it SHALL fail without applying and without crashing

