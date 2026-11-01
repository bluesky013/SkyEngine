## MODIFIED Requirements

### Requirement: Platform clock and worker synchronization
Each supported platform SHALL provide a nonzero monotonic clock frequency. Application timing SHALL be per instance and SHALL preserve precision when subtracting large counters. The Android worker semaphore SHALL block until permits are available and SHALL retain counted permits.

#### Scenario: Variable frame rate
- **WHEN** two consecutive frames are sampled on a 30 Hz or 120 Hz cadence
- **THEN** application delta SHALL reflect elapsed time rather than always using 1/60

#### Scenario: Counted worker wakeup
- **WHEN** three workers wait on a zero-count semaphore and two permits are signaled
- **THEN** exactly two workers SHALL return until another permit is signaled

#### Scenario: Shutdown with queued work
- **WHEN** a worker has queued signal closures or an application still has asset tasks during teardown
- **THEN** the worker SHALL finish before its semaphore is destroyed and asset work SHALL finish before its owning services and resources are destroyed; defining module code SHALL remain process resident
