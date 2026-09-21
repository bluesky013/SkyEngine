# navigation-async-query Specification

## Purpose
TBD - created by archiving change navigation-async-query. Update Purpose after archive.
## Requirements
### Requirement: Path result type

The navigation API SHALL provide a path result carrying world-space points and their straight-path flags.

#### Scenario: Query returns points

- **WHEN** a path query succeeds
- **THEN** the result SHALL contain ordered world-space points and matching flags

#### Scenario: Failed query returns no points

- **WHEN** a path query fails
- **THEN** the result SHALL be invalid with no points

### Requirement: Query filter factory

The navigation API SHALL provide a way to create a `NaviQueryFilter` without exposing backend types.

#### Scenario: Caller obtains a filter

- **WHEN** a caller requests a query filter from the factory
- **THEN** it SHALL receive a usable filter, or null when no backend is loaded

### Requirement: Synchronous query variant

A synchronous query SHALL remain available for tools and tests.

#### Scenario: Sync query fills a path

- **WHEN** the synchronous query is called with a valid filter
- **THEN** it SHALL fill the provided path result

### Requirement: Asynchronous path query

Path queries SHALL be submittable off the world tick, with a polled result, and SHALL be safe with respect to the
shared nav query.

#### Scenario: Polled completion

- **WHEN** an async path request is submitted
- **THEN** it SHALL report completion when finished and expose the resulting path

#### Scenario: Safe concurrent access

- **WHEN** multiple async queries run
- **THEN** access to the backend nav query SHALL be serialized so no data race occurs

#### Scenario: Query budget

- **WHEN** the number of in-flight queries reaches the budget
- **THEN** further requests SHALL be refused until earlier ones finish

### Requirement: Cancellation and invalidation

Async queries SHALL be cancellable, and a nav mesh change SHALL invalidate in-flight queries.

#### Scenario: Cancel a query

- **WHEN** an in-flight query is cancelled
- **THEN** it SHALL stop being applied and SHALL be discarded

#### Scenario: Mesh change invalidates queries

- **WHEN** the nav mesh changes
- **THEN** in-flight queries SHALL be cancelled

