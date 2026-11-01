## ADDED Requirements

### Requirement: Every active change has a disposition

The consolidation snapshot SHALL assign exactly one disposition to every directory under `openspec/changes/`
that is not in `archive/`, drawn from the fixed set `active`, `blocked-on-prereq`, `planned`, `frozen`,
`debt-record`, `superseded`, `ready-to-archive`. A planned item SHALL NOT be presented as implemented, and
ready-to-archive SHALL denote review readiness rather than archive authorization.

#### Scenario: Snapshot covers the full active set
- **WHEN** the snapshot is produced
- **THEN** it lists every active change directory and each entry carries one disposition from the fixed set

#### Scenario: Unknown disposition rejected
- **WHEN** a change is classified with a value outside the fixed set
- **THEN** the classification is invalid and MUST be corrected before the snapshot is accepted

### Requirement: Blocked changes name an existing prerequisite

A change with disposition `blocked-on-prereq` SHALL name at least one prerequisite that still exists as an
active change, archived change or explicitly parked backlog proposal.

#### Scenario: Prerequisite resolves to a real change
- **WHEN** a change is marked `blocked-on-prereq`
- **THEN** every named prerequisite resolves to an existing active, archived or explicitly parked backlog directory

#### Scenario: Prerequisite already archived
- **WHEN** a change's prerequisite has already been archived
- **THEN** the change MUST NOT be left as ordinary backlog; it is classified `active` or given a prerequisite
  that still exists

### Requirement: No dangling references in the snapshot

The snapshot SHALL NOT reference any change name that does not exist in the active set, archive or explicitly indexed backlog. Parked proposals SHALL remain planned or debt records rather than appearing delivered.

#### Scenario: Reference to nonexistent change
- **WHEN** a recorded relationship or note points at a change that does not exist
- **THEN** the reference MUST be replaced with a real change or spec name, or removed

### Requirement: Prerequisite-ordered workstream grouping

The snapshot SHALL group active changes into named workstreams and SHALL record the prerequisite ordering
between them.

#### Scenario: Workstream lists its members
- **WHEN** a workstream is recorded
- **THEN** it lists the active changes it contains

#### Scenario: Gating prerequisite is explicit
- **WHEN** one change gates another
- **THEN** the snapshot records the edge from the prerequisite to the dependent change

### Requirement: Cleanup checklist with per-item disposition

The snapshot SHALL include a cleanup checklist where each item states the target change, the intended action,
and whether user confirmation is required.

#### Scenario: Checklist item is actionable
- **WHEN** a cleanup item is recorded
- **THEN** it identifies the target change, the intended action, and whether confirmation is required

### Requirement: No destructive change without confirmation

This change SHALL NOT archive, delete, merge or park any existing change directory before explicit user confirmation of a concrete per-change plan. Domain consolidation and reversible documentation corrections MAY proceed within the authorized cleanup scope.

#### Scenario: Apply the confirmed cleanup plan
- **WHEN** the user has not yet approved the directory disposition plan
- **THEN** existing change directories remain in place and documentation may be corrected without claiming archive approval

#### Scenario: Preserve unfinished work after approval
- **WHEN** the user approves archive or backlog moves
- **THEN** execution SHALL follow the reviewed synchronization plan, retain transferred acceptance with its current owner, and preserve parked proposal content and incomplete status

### Requirement: Defined end state

Consolidation SHALL be considered complete only when every change has a current disposition and owner,
implementation work is distinguished from remaining acceptance, blocked changes have real prerequisites,
and planned/frozen/debt records and archive candidates are separated from current implementation work.

#### Scenario: End-state check
- **WHEN** the end-state criterion is evaluated
- **THEN** the active set has no unlabeled frozen/debt records and no change blocked on a nonexistent
  prerequisite

### Requirement: Canonical domain specifications preserve existing contracts

Main specifications SHALL use the canonical domain IDs in the capability migration map. Domain consolidation SHALL preserve every existing requirement body, resolve colliding names with explicit source scope, and migrate active delta paths and concrete references together. Archived snapshots SHALL retain their historical IDs.

#### Scenario: Consolidate existing leaf capabilities

- **WHEN** multiple leaf capabilities are merged into one domain specification
- **THEN** every original requirement SHALL resolve to one uniquely named domain requirement with equivalent content, and the recorded mapping SHALL locate its former capability

### Requirement: Draft plans remain distinct from current specifications

Canonical main specs SHALL pass strict validation. An incomplete proposal without a designed delta SHALL remain explicitly planned or debt-record and SHALL NOT acquire invented requirements merely to silence validation. Future deltas SHALL NOT be promoted into current main contracts based solely on checked task counts.

#### Scenario: Inspect an unexpanded proposal

- **WHEN** a proposal has no delta specifications
- **THEN** the maintenance report SHALL identify its missing design boundary separately from valid current specifications and implementation-ready changes

### Requirement: Archive previews reconcile the newest contract

Before requesting archive approval, the maintenance report SHALL distinguish safe synchronization, already-applied requirements and historical or superseded deltas. A former contract SHALL NOT overwrite a newer process-residency, ownership, backend-configuration or typed-shader contract. Transferred acceptance SHALL have one named current owner and SHALL remain uncompleted until verified.

#### Scenario: Review an older completed change

- **WHEN** its delta differs from the current main spec because a later contract supersedes it
- **THEN** the preview SHALL preserve the newer contract, identify any still-missing valid requirement or scenario, and require explicit confirmation before moving the change to archive
