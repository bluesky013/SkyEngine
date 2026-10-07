## ADDED Requirements

### Requirement: Every active change has a disposition
The consolidation snapshot SHALL assign exactly one disposition to every directory under
`openspec/changes/` that is not in `archive/`.

#### Scenario: Snapshot covers the full active set
- **WHEN** the snapshot is produced
- **THEN** it lists every active change directory and each entry carries a disposition

#### Scenario: Undispositioned change is a failure
- **WHEN** an active change directory has no disposition in the snapshot
- **THEN** the snapshot is considered incomplete and the change is not consolidated

### Requirement: Dispositions come from a fixed vocabulary
Each disposition SHALL be one of `active`, `blocked-on-prereq`, `frozen`, `debt-record`, or `superseded`.

#### Scenario: Unknown disposition rejected
- **WHEN** a change is classified with a value outside the fixed vocabulary
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

#### Scenario: Known dangling references are resolved
- **WHEN** the snapshot is produced
- **THEN** the previously dangling references (`animation-graph-assets`, `aurora-animation-bridge`,
  `navigation-path-query`, `aurora-material-pso`) are each mapped to a real change or spec, or removed

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

### Requirement: No archive without confirmation
The consolidation change SHALL NOT archive, delete, or merge any existing change directory; archiving requires
explicit user confirmation.

#### Scenario: Planning-only guarantee
- **WHEN** this change is applied
- **THEN** no change directory other than `consolidate-active-changes` is modified or archived

### Requirement: Defined end state
Consolidation SHALL be considered complete only when the active set contains solely `active` and
`blocked-on-prereq` changes with existing prerequisites, and every `frozen`/`debt-record` change is explicitly
labeled as not scheduled.

#### Scenario: End-state check
- **WHEN** the end-state criterion is evaluated
- **THEN** the active set has no unlabeled frozen/debt records and no change blocked on a nonexistent
  prerequisite
