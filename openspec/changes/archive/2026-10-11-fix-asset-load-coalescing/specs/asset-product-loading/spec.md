## MODIFIED Requirements

### Requirement: Concurrency safety for load and cook

Loading and cooking SHALL not deadlock or lose state: an in-process cook MUST NOT occupy a loader worker while waiting, `product.index` writes SHALL be serialized per bundle, and a LOADING asset's wait handle SHALL be established before its cook is scheduled. On-demand coalescing SHALL be **atomic**: concurrent loads of the same UUID SHALL reuse the existing wait handle and MUST NOT overwrite it, exactly one cook SHALL be scheduled per in-flight UUID, and a scheduled cook SHALL NOT be dropped while a waiter depends on it. Reading the wait handle SHALL be synchronized (no unsynchronized read).

#### Scenario: No loader-pool starvation

- **WHEN** an on-demand cook is scheduled from a load running on the asset pool
- **THEN** the cook SHALL run without occupying a loader worker, so the pool cannot deadlock

#### Scenario: Concurrent index updates do not lose entries

- **WHEN** multiple cooks complete for the same bundle
- **THEN** each `product.index` update SHALL be serialized so no entry is lost

#### Scenario: Wait handle ready before cook

- **WHEN** a load returns a LOADING asset
- **THEN** its wait handle SHALL already be established so a blocking wait unblocks on LOADED or FAILED

#### Scenario: Concurrent loads coalesce without orphaning a waiter

- **WHEN** multiple threads load the same unbuilt asset concurrently
- **THEN** exactly one cook SHALL be scheduled, every caller SHALL share the same wait handle, and every blocking wait SHALL complete when the single cook finishes (no overwritten/orphaned wait handle)

#### Scenario: Late caller after a completed load does not re-cook

- **WHEN** a caller requests an asset whose load has already completed (LOADED or FAILED)
- **THEN** it SHALL NOT start a redundant second cook; a LOADED asset is returned as-is and a FAILED asset may be re-attempted
