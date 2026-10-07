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

Constraints: `AGENTS.md` requires explicit user confirmation before archiving, and requires spec and code for a
change to ship together. This change is therefore **planning-only**: it records the snapshot and the cleanup
checklist, and executes none of the cleanup.

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
| WS-PYTHON | Python static runtime | `add-static-python-embedding`, `add-android-python-runtime`, `add-python-ssl` |
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
| `add-static-python-embedding` | active | 20/24; foundation of WS-PYTHON |
| `add-android-python-runtime` | blocked-on-prereq | on `add-static-python-embedding` |
| `add-python-ssl` | blocked-on-prereq | on `add-static-python-embedding`, `add-android-python-runtime` |
| `animation-events` | active | 0/10 but unblocked; gates `audio-animation-bridge` |
| `audio-animation-bridge` | blocked-on-prereq | on `animation-events` |
| `audio-world-integration` | blocked-on-prereq | on `aurora-renderer` (world/scene attach path) |
| `aurora-renderer` | active (needs completion) | proposal-only; no `tasks.md`/`design.md`/`specs/` |
| `aurora-scene-bridge` | blocked-on-prereq | on `aurora-renderer`; seam overlap to resolve |
| `aurora-metal-bindless-descriptor-heap` | debt-record | long-horizon; upstream slang dependency |
| `aurora-cook-schema-layering` | debt-record | layering debt; no spec delta |
| `legacy-render-thirdparty-cleanup` | debt-record | blocked on legacy render retirement; no tasks/specs |
| `lazy-transform-update` | frozen | explicit "Frozen (not scheduled)" |
| `asset-pipeline` | active (+ superseded rows) | 48/62; IPC task rows delivered by archived `asset-cook-ipc` |
| `navigation-terrain-build-tests` | active | test-only; prereq `terrain-navigation-integration` archived |
| `terrain-aurora-render` | blocked-on-prereq | on `aurora-renderer` |
| `terrain-editor-tools` | blocked-on-prereq | on aurora sandbox editor + `terrain-aurora-render` |
| `vegetation-aurora-render` | blocked-on-prereq | on `aurora-renderer` |
| `vegetation-editor-tools` | blocked-on-prereq | on aurora sandbox editor + `vegetation-aurora-render` |
| `add-network-prediction` | active | prereqs archived; needs rebase decision vs `network-lockstep` |
| `aurora-navigation-integration` | blocked-on-prereq | on `aurora-renderer`; missing spec/design |

### Decision: Prerequisite graph

- `add-static-python-embedding` -> `add-android-python-runtime` -> `add-python-ssl`
- `aurora-renderer` -> `aurora-scene-bridge`, `terrain-aurora-render`, `vegetation-aurora-render`,
  `audio-world-integration`, `aurora-navigation-integration`
- `terrain-aurora-render` -> `terrain-editor-tools`
- `vegetation-aurora-render` -> `vegetation-editor-tools`
- `animation-events` -> `audio-animation-bridge`

The single highest-leverage prerequisite is `aurora-renderer`: it gates the whole WS-WORLD workstream plus the
audio-world and navigation-integration attach paths. It is proposal-only, so making it actionable is the primary
unblocking action.

### Decision: Cleanup checklist carried by this change (not executed here)

- **C1 — Park frozen/debt changes (DECIDED: keep as labeled references; no archive).** `lazy-transform-update`
  (frozen) and the three debt-records stay in place with an explicit "Frozen / Debt record - not scheduled"
  banner. Archiving is NOT performed. Revisit only if the underlying trigger fires.
- **C2 — Complete or fold proposal-only changes.** `aurora-renderer` must gain `design.md`/`specs/`/`tasks.md`
  before it is actionable (owned by the user); `legacy-render-thirdparty-cleanup` stays a debt-record until
  legacy render retires.
- **C3 — Fix dangling references (DONE for the three active ones).** `animation-events`,
  `audio-animation-bridge`, and `aurora-navigation-integration` now read "not yet a change" instead of naming a
  nonexistent change. `aurora-renderer` -> `aurora-material-pso` is owned by the user (handling
  `aurora-renderer` directly).
- **C4 — Trim superseded task rows (DONE).** `asset-pipeline` tasks 10.4/10.5/10.6c/14.4/14.5 are marked
  delivered by archived `asset-cook-ipc`; the "sandbox editor refactor" deferrals are marked
  "no tracking change yet".
- **C5 — Rebase network prediction (DECIDED: coexist; prediction is the active model).** `add-network-prediction`
  is the current line for the replication bridge; archived `network-lockstep` remains an alternative model and
  is not retired.
- **C6 — Resolve the scene-seam overlap (DECIDED: `aurora-renderer` owns the seam).** `aurora-renderer` owns the
  main-thread scene handle + command queue (`RenderSceneProxy`); `aurora-scene-bridge` only transfers framework
  component data into the aurora scene ECS and MUST NOT add a second scene proxy.
- **C7 — Missing artifacts (DECIDED: leave; no new tracking changes).** The not-yet-existing follow-ups
  (animation graph assets, aurora animation bridge, navigation path query, and the "sandbox editor refactor")
  stay as clearly labeled "not yet a change" notes.

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

## Open Questions

- None blocking. Revisit the WS-AURORA hub shape only if `aurora-renderer`'s own design (owned by the user)
  changes the render-loop vs scene-seam split.
