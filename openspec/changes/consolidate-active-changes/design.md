## Context

`openspec/changes/` currently holds 20 active change directories on top of a large archive (200+ entries).
Reading the active set shows four problems:

1. **Prerequisites are already archived but the dependent change is still listed as ordinary backlog**
   (e.g. `add-network-prediction` sits on archived `add-network-core`/`add-network-replication`;
   `aurora-scene-bridge` sits on archived `harden-framework-components`).
2. **Frozen / debt records are mixed in with scheduled work** (`lazy-transform-update` is explicitly frozen;
   `aurora-cook-schema-layering`, `aurora-metal-bindless-descriptor-heap`, `legacy-render-thirdparty-cleanup`
   declare themselves "proposal (unimplemented)").
3. **Overlaps and splits** (two framework-to-aurora scene seams; taskflow/boost removal spread over three
   changes).
4. **Dangling references** to changes that do not exist (`animation-graph-assets`, `aurora-animation-bridge`,
   `navigation-path-query`, `aurora-material-pso`).
5. **Live drift during this change**: `add-static-python-embedding` was archived mid-flight (its dependents are
   now unblocked) and task counts moved, confirming the snapshot must be re-derived, not treated as frozen truth.

Constraints: `AGENTS.md` requires explicit user confirmation before archiving, and requires spec and code for a
change to ship together. This change records the snapshot and the cleanup checklist; the non-destructive cleanup
items (C1 parking labels, C3 reference rewording, C4 superseded-row trimming, C5-C7 decision records) have been
applied with user approval. No change directory was archived.

## Goals / Non-Goals

**Goals:**

- Give every active change exactly one disposition from a fixed vocabulary.
- Group active changes into prerequisite-ordered workstreams and make the real gating graph explicit.
- Record a concrete, itemized cleanup checklist (each item with a disposition and whether it needs confirmation).
- Define the end-state criterion for "consolidated".

**Non-Goals:**

- No engine/plugin/CMake/third-party code and no runtime behavior change.
- Do not modify, archive, or merge any other change directory in this change.
- Do not re-litigate capability designs of the underlying changes.

## Decisions

### Decision: Fixed disposition vocabulary

Each active change is classified as one of:

- **active** — prerequisites met, may proceed now.
- **blocked-on-prereq** — valid, but gated by a named, still-existing prerequisite.
- **frozen** — an explicit decision not to schedule; retained for reference.
- **debt-record** — an unimplemented record of known debt/roadmap; reference only until triggered.
- **superseded** — its content is (partly) delivered elsewhere; needs trimming, not implementation here.

Alternatives considered: a free-form prose summary (rejected: not checkable), or a single "keep/delete" flag
(rejected: loses the frozen-vs-debt distinction that drives the cleanup action).

### Decision: Eight workstreams

| ID | Workstream | Members |
|----|------------|---------|
| WS-PYTHON | Python static runtime | `add-android-python-runtime`, `add-python-ssl` (base `add-static-python-embedding` archived) |
| WS-AURORA | Aurora render loop (hub) | `aurora-renderer`, `aurora-scene-bridge`, `aurora-navigation-integration` |
| WS-WORLD | Terrain / vegetation render + editor tools | `terrain-aurora-render`, `terrain-editor-tools`, `vegetation-aurora-render`, `vegetation-editor-tools`, `navigation-terrain-build-tests` |
| WS-AUDIO | Audio / animation | `animation-events`, `audio-animation-bridge`, `audio-world-integration` |
| WS-NET | Network | `add-network-prediction` |
| WS-ASSET | Asset pipeline | `asset-pipeline` |
| WS-RHI | Aurora RHI backlog | `aurora-metal-bindless-descriptor-heap` |
| WS-DEBT | Framework / build debt (parked) | `lazy-transform-update`, `legacy-render-thirdparty-cleanup`, `aurora-cook-schema-layering` |

### Decision: Classification snapshot

| Change | Disposition | Notes |
|--------|-------------|-------|
| `add-android-python-runtime` | active | 14/20; prereq `add-static-python-embedding` archived |
| `add-python-ssl` | active | 15/20; prereq `add-static-python-embedding` archived |
| `animation-events` | active | 0/10 but unblocked; gates `audio-animation-bridge` |
| `audio-animation-bridge` | blocked-on-prereq | on `animation-events` |
| `audio-world-integration` | blocked-on-prereq | on `aurora-renderer` (world/scene attach path) |
| `aurora-renderer` | active (needs completion) | proposal-only; no `tasks.md`/`design.md`/`specs/` |
| `aurora-scene-bridge` | blocked-on-prereq | on `aurora-renderer`; seam overlap to resolve |
| `aurora-metal-bindless-descriptor-heap` | debt-record | long-horizon; upstream slang dependency |
| `aurora-cook-schema-layering` | debt-record | layering debt; no spec delta |
| `legacy-render-thirdparty-cleanup` | debt-record | blocked on legacy render retirement; no tasks/specs |
| `lazy-transform-update` | frozen | explicit "Frozen (not scheduled)" |
| `asset-pipeline` | active (superseded rows trimmed) | 51/59; IPC task rows delivered by archived `asset-cook-ipc` |
| `navigation-terrain-build-tests` | active | test-only; prereq `terrain-navigation-integration` archived |
| `terrain-aurora-render` | blocked-on-prereq | on `aurora-renderer` |
| `terrain-editor-tools` | blocked-on-prereq | on aurora sandbox editor + `terrain-aurora-render` |
| `vegetation-aurora-render` | blocked-on-prereq | on `aurora-renderer` |
| `vegetation-editor-tools` | blocked-on-prereq | on aurora sandbox editor + `vegetation-aurora-render` |
| `add-network-prediction` | active | prereqs archived; needs rebase decision vs `network-lockstep` |
| `aurora-navigation-integration` | blocked-on-prereq | on `aurora-renderer`; missing spec/design |

### Decision: Prerequisite graph

- `add-static-python-embedding` (ARCHIVED) -> `add-android-python-runtime` -> `add-python-ssl` (both now active)
- `aurora-renderer` -> `aurora-scene-bridge`, `terrain-aurora-render`, `vegetation-aurora-render`,
  `audio-world-integration`, `aurora-navigation-integration`
- `terrain-aurora-render` -> `terrain-editor-tools`
- `vegetation-aurora-render` -> `vegetation-editor-tools`
- `animation-events` -> `audio-animation-bridge`

The single highest-leverage prerequisite is `aurora-renderer`: it gates the whole WS-WORLD workstream plus the
audio-world and navigation-integration attach paths. It is proposal-only, so making it actionable is the primary
unblocking action.

### Decision: Cleanup checklist (C1-C7; decisions recorded)

- **C1 — Park frozen/debt (decided): labeled "not scheduled", no archive.**
- **C2 — Proposal-only: `aurora-renderer` needs design/specs/tasks (owner: user).**
- **C3 — Dangling refs (done): 3 active ones reworded to "not yet a change"; `aurora-renderer` ref owner: user.**
- **C4 — Superseded rows (done): `asset-pipeline` 10.4/10.5/10.6c/14.4/14.5 marked delivered by archived `asset-cook-ipc`; deferrals marked "no tracking change yet".**
- **C5 — Network (decided): `add-network-prediction` coexists with archived `network-lockstep`; prediction is the active model.**
- **C6 — Scene seam (decided): `aurora-renderer` owns `RenderSceneProxy`; `aurora-scene-bridge` transfers data only.**
- **C7 — Follow-ups (decided): no new tracking changes; missing items stay as "not yet a change" notes.**

### Decision: End-state criterion

Consolidation is complete when the active set contains only changes that are `active` or
`blocked-on-prereq` with a named, existing prerequisite; every `frozen`/`debt-record` change is explicitly
labeled as not scheduled (or archived); and no active change references a nonexistent change.

## Risks / Trade-offs

- [Archiving frozen/debt changes loses recorded rationale] -> Mitigation: prefer label-as-parked over archive;
  if archived, first capture the rationale in `openspec/specs/` or this design.
- [Physical merge of directories is risky and loses history] -> Mitigation: explicitly deferred; this change
  only classifies and lists.
- [The snapshot goes stale quickly] -> Mitigation: treat the checklist as tasks that are re-validated when
  executed, not a frozen truth.
- [Unresolved `aurora-renderer` overlap blocks the largest workstream] -> Mitigation: make completing
  `aurora-renderer` and resolving C6 the top-priority unblocking tasks.

## Migration Plan

1. Record the snapshot (this change's artifacts).
2. For each checklist item, perform the operation separately with explicit confirmation (C1 requires user
   confirmation before any archive).
3. Re-derive the active list and confirm the end-state criterion holds. Rollback: no destructive operation is
   performed by this change, so reverting is a no-op.
