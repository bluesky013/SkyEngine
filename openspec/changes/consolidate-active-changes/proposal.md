## Why

`openspec/changes/` has accumulated **20 active changes** that no longer describe a coherent backlog. Several
hard-depend on work already archived (so their prerequisite is done but they sit at 0 tasks), several are explicit
**frozen** or **unimplemented debt records** that will never be scheduled as written, some overlap each other
(two different framework→aurora scene seams; taskflow/boost removal split across three changes), and several
reference changes that do not exist (`animation-graph-assets`, `aurora-animation-bridge`, `navigation-path-query`,
`aurora-material-pso`). The result is that the backlog cannot answer "what is actually next?" or "what is
blocked on what?".

This change is a **planning-first consolidation**: it takes stock of the current state, groups the active changes
into prerequisite-ordered workstreams, and records a concrete cleanup checklist. The non-destructive cleanup items
(reference rewording, superseded-row trimming, parking labels, decision records) have since been applied with user
approval; no change directory was archived, and no engine or third-party code is touched.

## What Changes

- Record a single **current-state snapshot**: each active change classified as `active`, `blocked-on-prereq`,
  `frozen`, `debt-record`, or `superseded`, with task counts and the prerequisites it actually depends on.
- Group the active changes into **workstreams** (Python runtime, audio/animation, aurora render migration,
  terrain, vegetation, network, asset pipeline, framework debt) and state the real prerequisite ordering between
  them (e.g. the aurora render loop gates terrain/vegetation render).
- Record a **cleanup checklist** with an explicit disposition per item, marking applied ones:
  - archive or keep-as-frozen the frozen / debt-record changes;
  - flesh out or fold the proposal-only changes that have no `tasks.md`/`specs/`;
  - fix dangling references to nonexistent changes;
  - trim/superseded task rows that archived changes already delivered (notably in `asset-pipeline`);
  - re-point deferrals that cite an unnamed "sandbox editor refactor" to a real tracking change.
- Define the intended end state so the backlog shrinks to a set where every entry is either scheduled,
  explicitly frozen-by-decision, or blocked on a named prerequisite.

**Non-goals**: no engine code, no CMake/third-party changes, no physical merge of change directories, and no
archiving. Only non-destructive doc edits to other changes (references, superseded rows, parking banners) apply.

## Capabilities

### New Capabilities

- `change-backlog-consolidation`: classification and disposition rules for active OpenSpec changes, the
  prerequisite-ordered workstream grouping, and the requirements the recorded consolidation snapshot must
  satisfy (every change has a disposition, no dangling references remain in the snapshot, blocked changes name
  a real prerequisite).

### Modified Capabilities

<!-- none: this is a process/backlog capability; no existing engine capability requirements change. -->

## Impact

- `openspec/` only: this change's own artifacts plus the classification of the 20 existing active changes.
- No engine, plugin, CMake, or third-party impact. No runtime behavior change.
- Downstream: any remaining cleanup (archiving, if ever) stays a separate, user-confirmed maintenance operation.
