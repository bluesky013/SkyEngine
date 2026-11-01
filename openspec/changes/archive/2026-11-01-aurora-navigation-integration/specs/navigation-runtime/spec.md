## ADDED Requirements

### Requirement: Validated runtime settings and lifetime

The runtime SHALL apply reflected settings to meshes. Invalid/mismatched settings SHALL reject creation with a diagnostic. Registration SHALL retain callback code leases and shutdown SHALL remove only session-owned registrations.

#### Scenario: Custom settings

- **WHEN** a world configures custom resolution and agent dimensions
- **THEN** generation SHALL use those values

#### Scenario: Invalid settings

- **WHEN** settings contain invalid or non-finite dimensions
- **THEN** navigation creation SHALL fail explicitly without assertions or exceptions

#### Scenario: Retained objects

- **WHEN** the module shuts down with referenced meshes/tasks
- **THEN** new factory creation SHALL stop and retained objects SHALL keep implementation code alive

### Requirement: Cooked mesh runtime binding

The reflected navigation config SHALL accept a NaviMesh asset reference and choose eager loading or tile streaming. Runtime initialization SHALL verify asset loading and backend installation explicitly. An empty reference SHALL preserve programmatic generation. Offline cooking SHALL NOT load a previous product as its input navigation mesh.

#### Scenario: Eager cooked mesh

- **WHEN** a valid cooked mesh is configured with streaming disabled
- **THEN** runtime initialization SHALL load an independent queryable mesh before world attachment

#### Scenario: Streamed cooked mesh

- **WHEN** a tiled mesh is configured with streaming enabled
- **THEN** runtime initialization SHALL own its manifest and page it around the streaming focus

#### Scenario: Missing configured mesh

- **WHEN** the asset is missing, fails loading or cannot be installed
- **THEN** navigation initialization SHALL fail with a diagnostic
