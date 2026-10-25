## ADDED Requirements

### Requirement: Explicit-result atomic product group publication
The skeletal import path SHALL publish each owner-source/bundle Mesh/Skin/Skeleton group through an explicit Result-returning staging/commit API in the existing Framework asset pipeline. It SHALL write a new immutable generation, verify all required writes, flushes, encoding and readback, and publish a complete group manifest through one atomic replacement. No member SHALL be visible as part of the new generation before that commit. Failed or stale-generation cooks SHALL NOT overwrite the previous committed group. Updating several UUID files independently SHALL NOT be considered an atomic group commit. Unsupported filesystem guarantees SHALL return an explicit error.

#### Scenario: Failure after writing a skin
- **WHEN** Skin staging succeeds but Skeleton staging or group commit fails
- **THEN** all previously committed group members SHALL remain resolvable and no success event SHALL be emitted for the failed new generation

#### Scenario: Process stops before commit
- **WHEN** the cook process exits after staging files but before replacing the group manifest
- **THEN** subsequent loads SHALL use the previous complete committed generation and SHALL ignore uncommitted staging files

#### Scenario: Older cook finishes late
- **WHEN** an outdated request completes after a newer generation has committed for the same owner and bundle
- **THEN** it SHALL NOT replace the newer manifest or report its contents as the latest version

### Requirement: Immutable product revisions during binding and reload
New skeletal workflow products SHALL resolve through the existing product loader with a pinned group revision for group members and a validated exact revision tuple for independent Clip/Graph dependencies. Reload SHALL construct new asset snapshots rather than mutate published data or open bulk streams. Existing consumers SHALL retain their exact content while referenced. Clip/Graph single-product publication SHALL also return explicit success only after staging and commit. Runtime resolution and packaging SHALL use product metadata without source files or source catalogs.

#### Scenario: Reload during a group read
- **WHEN** a new group commits between loading its Mesh and Skin
- **THEN** an existing binding request SHALL finish against its pinned generation or restart the entire candidate, never combine arbitrary latest members

#### Scenario: Failed graph write
- **WHEN** Graph serialization, compression or file publication fails
- **THEN** the save/cook result SHALL report failure and the previous successful product SHALL remain readable

#### Scenario: Products-only package
- **WHEN** a World referencing separate Mesh/Skin/Skeleton assets and a Graph is packaged without sources
- **THEN** all explicit component roots, transitive Clip/Graph dependencies and required group resolution metadata SHALL resolve through AssetManager

### Requirement: Generated child cook requests complete through their owner
A missing generated child with a valid catalog owner SHALL request the owner source cook, coalesced by owner and target bundle. Every waiting child SHALL receive success only when its own requested member exists in the committed group; failure or a removed child SHALL complete as Failed with a diagnostic. Both in-process and out-of-process completion SHALL identify the affected child UUIDs and generation. No child request SHALL recursively schedule a nonexistent child source or remain Loading indefinitely.

#### Scenario: Concurrent skin and skeleton requests
- **WHEN** an uncooked Skin and Skeleton from the same model are requested together
- **THEN** one owner cook per bundle SHALL serve both requests and both SHALL reach a terminal result

#### Scenario: Requested child removed by reimport
- **WHEN** the owner cooks successfully but the requested child no longer exists
- **THEN** that child's waiter SHALL fail explicitly instead of inheriting the owner's success or remaining Loading
