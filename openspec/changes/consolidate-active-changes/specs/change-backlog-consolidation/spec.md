## ADDED Requirements

### Requirement: Every active change has a disposition
The consolidation snapshot SHALL assign exactly one disposition to every directory under `openspec/changes/`
that is not in `archive/`, drawn from the fixed set `active`, `blocked-on-prereq`, `frozen`, `debt-record`,
`superseded`.

#### Scenario: Snapshot covers the full active set
- **WHEN** the snapshot is produced
- **THEN** it lists every active change directory and each entry carries one disposition from the fixed set

#### Scenario: Unknown disposition rejected
- **WHEN** a change is classified with a value outside the fixed set
- **THEN** the classification is invalid and MUST be corrected before the snapshot is accepted

### Requirement: Blocked changes name an existing prerequisite
A change with disposition `blocked-on-prereq` SHALL name at least one prerequisite that still exists as an
active change or an archived change.

#### Scenario: Prerequisite resolves to a real change
- **WHEN** a change is marked `blocked-on-prereq`
- **THEN** every named prerequisite resolves to an existing active or archived change directory

#### Scenario: Prerequisite already archived
- **WHEN** a change's prerequisite has already been archived
- **THEN** the change MUST NOT be left as ordinary backlog; it is classified `active` or given a prerequisite
  that still exists

### Requirement: No dangling references in the snapshot
The snapshot SHALL NOT reference any change name that does not exist in the active set or the archive.

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
This change SHALL NOT archive, delete, or merge any existing change directory; those operations require
explicit user confirmation.

#### Scenario: Non-destructive-only guarantee
- **WHEN** this change is applied
- **THEN** no change directory is archived, deleted, or merged, and any edits to other changes are limited to
  non-destructive documentation (references, task-row status, status banners)

### Requirement: Defined end state
Consolidation SHALL be considered complete only when the active set contains solely `active` and
`blocked-on-prereq` changes with existing prerequisites, and every `frozen`/`debt-record` change is explicitly
labeled as not scheduled.

#### Scenario: End-state check
- **WHEN** the end-state criterion is evaluated
- **THEN** the active set has no unlabeled frozen/debt records and no change blocked on a nonexistent
  prerequisite
