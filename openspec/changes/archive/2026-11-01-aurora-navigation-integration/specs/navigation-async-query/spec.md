## MODIFIED Requirements

### Requirement: Asynchronous path query

Path queries SHALL be submittable off the world tick, with a polled result. Submission and service configuration SHALL occur on the world thread, while polling and cancellation SHALL be safe across threads. Submission SHALL freeze filter values before returning. Concurrent backend readers SHALL use independent Detour search contexts with shared mesh read protection; native topology mutation SHALL exclude all readers.

#### Scenario: Polled completion

- **WHEN** an async path request is submitted
- **THEN** it SHALL report completion when finished and expose the resulting path

#### Scenario: Safe concurrent access

- **WHEN** multiple async queries run
- **THEN** their mutable search contexts SHALL be independent and read-only mesh access SHALL be allowed to overlap

#### Scenario: Query budget

- **WHEN** the number of physically unfinished tracked queries reaches the budget
- **THEN** further requests SHALL be refused until earlier ones actually finish, including cancellation-requested queries

#### Scenario: Caller edits its filter after submission

- **WHEN** a caller changes filter flags or costs after RequestPath returns
- **THEN** the queued request SHALL use its submission-time values without reading the caller's later changes

### Requirement: Cancellation and invalidation

Async queries SHALL be cancellable, and a nav mesh change SHALL invalidate in-flight queries. Cancellation and terminal publication SHALL have one atomic ordering. A cancellation request SHALL NOT imply backend work has ended or release its admission slot. A terminal result SHALL remain immutable.

#### Scenario: Cancel a query

- **WHEN** an in-flight query is cancelled before terminal publication
- **THEN** it SHALL stop being applied, finish with invalid empty output and keep its slot until actual completion

#### Scenario: Mesh change invalidates queries

- **WHEN** the nav mesh changes
- **THEN** in-flight queries SHALL be cancelled

#### Scenario: Cancellation after completion

- **WHEN** Cancel is called after a successful terminal result is published
- **THEN** success and its path SHALL remain unchanged
