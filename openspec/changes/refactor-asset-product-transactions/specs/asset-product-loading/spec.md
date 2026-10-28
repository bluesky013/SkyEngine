## MODIFIED Requirements

### Requirement: Generated child requests have individual terminal results
Framework SHALL route a generated child cook request to its source owner and coalesce concurrent requests by owner and target. Every child waiter SHALL complete according to the presence and validity of its own requested member. Failed cooks and removed children SHALL produce explicit failure rather than indefinite Loading or inherited owner success.

#### Scenario: Concurrent children of one source
- **WHEN** two generated children require the same owner cook and target
- **THEN** one cook serves both requests and each child receives its own terminal result

#### Scenario: Child omitted by a successful cook
- **WHEN** the owner succeeds but the requested child no longer exists
- **THEN** that child completes with failure
