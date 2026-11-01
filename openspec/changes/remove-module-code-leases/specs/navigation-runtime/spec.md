## MODIFIED Requirements

### Requirement: Validated runtime settings and lifetime

The runtime SHALL apply reflected settings to meshes. Invalid/mismatched settings SHALL reject creation with a diagnostic. Provider DLLs SHALL remain process-resident without callback code leases, and shutdown SHALL remove only session-owned registrations.

#### Scenario: Custom settings

- **WHEN** a world configures custom resolution and agent dimensions
- **THEN** generation SHALL use those values

#### Scenario: Invalid settings

- **WHEN** settings contain invalid or non-finite dimensions
- **THEN** navigation creation SHALL fail explicitly without assertions or exceptions

#### Scenario: Retained objects

- **WHEN** the module shuts down with referenced meshes/tasks
- **THEN** new factory creation SHALL stop and retained meshes, generators and tasks SHALL preserve their data ownership and completion constraints while implementation code remains process-resident
