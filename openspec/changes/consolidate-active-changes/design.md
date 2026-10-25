> 2026-10-26：旧 RenderGraph 及其 PipelinePass 模板已退役；相关旧模板目标不再实施，参见 `remove-unused-legacy-rendergraph`。其余目标维持原验收状态。

## Context

`openspec/changes/` currently holds **23 active** change directories on top of a large archive (151 entries).
Reading the active set shows five problems:

1. **Prerequisites are already archived but the dependent change is still listed as ordinary backlog**
   (e.g. `add-network-prediction` sits on archived `add-network-core`/`add-network-replication`;
   `aurora-scene-bridge` sits on archived `harden-framework-components`; `add-static-module-bundling` sits on
   archived `add-project-native-layout`).
2. **Frozen / debt records are mixed in with scheduled work** (`lazy-transform-update` is explicitly frozen;
   `aurora-cook-schema-layering` and `aurora-metal-bindless-descriptor-heap` declare themselves
   "proposal (unimplemented)"; `legacy-render-thirdparty-cleanup` has since become a scheduled retirement change).
3. **Overlaps and splits** (two framework-to-aurora scene seams; taskflow/boost removal spread over three
   changes).
4. **Dangling references** to changes that do not exist (`animation-graph-assets`, `aurora-animation-bridge`,
   `navigation-path-query`, `aurora-material-pso`).
5. **Live drift**: the snapshot must be re-derived, not treated as frozen truth. Since the first snapshot the set
   changed materially: `asset-pipeline` and `add-static-python-embedding` were archived; `export-preset-schema-source`
   was created and archived; and `add-game-project-build`, `add-static-module-bundling`, `add-ios-platform`,
   `add-ios-uikit-backend` joined the active set.

Constraints: `AGENTS.md` requires explicit user confirmation before archiving, and requires spec and code for a
change to ship together. This change records the snapshot and the cleanup checklist; the non-destructive cleanup
items (C1 parking labels, C3 reference rewording, C4 superseded-row trimming, C5-C7 decision records) have been
applied with user approval. No change directory was archived by this change.

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

### Decision: Nine workstreams

| ID | Workstream | Members |
|----|------------|---------|
| WS-PYTHON | Python static runtime | `add-android-python-runtime`, `add-python-ssl` (base `add-static-python-embedding` archived) |
| WS-AURORA | Aurora render loop (hub) | `aurora-renderer`, `aurora-scene-bridge`, `aurora-navigation-integration` |
| WS-WORLD | Terrain / vegetation render + editor tools | `terrain-aurora-render`, `terrain-editor-tools`, `vegetation-aurora-render`, `vegetation-editor-tools`, `navigation-terrain-build-tests` |
| WS-AUDIO | Audio / animation | `animation-events`, `audio-animation-bridge`, `audio-world-integration` |
| WS-NET | Network | `add-network-prediction` |
| WS-BUILD | Packaging / platform / build | `add-game-project-build`, `add-static-module-bundling`, `add-ios-platform`, `add-ios-uikit-backend` |
| WS-RHI | Aurora RHI backlog | `aurora-metal-bindless-descriptor-heap` |
| WS-DEBT | Framework / build debt (parked) | `lazy-transform-update`, `aurora-cook-schema-layering` |
| WS-META | Backlog hygiene | `consolidate-active-changes` |

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
| `aurora-navigation-integration` | blocked-on-prereq | on `aurora-renderer`; missing spec/design |
| `aurora-metal-bindless-descriptor-heap` | debt-record | long-horizon; upstream slang dependency |
| `aurora-cook-schema-layering` | debt-record | layering debt; no spec delta |
| `legacy-render-thirdparty-cleanup` | active | scheduled legacy render/shader/editor + third-party retirement; 0/29 |
| `lazy-transform-update` | frozen | explicit "Frozen (not scheduled)" |
| `navigation-terrain-build-tests` | active | test-only; prereq `terrain-navigation-integration` archived |
| `terrain-aurora-render` | blocked-on-prereq | on `aurora-renderer` |
| `terrain-editor-tools` | blocked-on-prereq | on aurora sandbox editor + `terrain-aurora-render` |
| `vegetation-aurora-render` | blocked-on-prereq | on `aurora-renderer` |
| `vegetation-editor-tools` | blocked-on-prereq | on aurora sandbox editor + `vegetation-aurora-render` |
| `add-network-prediction` | active | prereqs archived; needs rebase decision vs `network-lockstep` |
| `add-game-project-build` | active | 17/27; in progress |
| `add-static-module-bundling` | active | 0/16; prereq `add-project-native-layout` archived |
| `add-ios-platform` | active | 0/10 |
| `add-ios-uikit-backend` | active | 8/13 |
| `consolidate-active-changes` | active (meta) | this change |

### Decision: Prerequisite graph

- `add-static-python-embedding` (ARCHIVED) -> `add-android-python-runtime` -> `add-python-ssl`
- `aurora-renderer` -> `aurora-scene-bridge`, `terrain-aurora-render`, `vegetation-aurora-render`,
  `audio-world-integration`, `aurora-navigation-integration`
- `terrain-aurora-render` -> `terrain-editor-tools`
- `vegetation-aurora-render` -> `vegetation-editor-tools`
- `animation-events` -> `audio-animation-bridge`
- `add-project-native-layout` (ARCHIVED) -> `add-static-module-bundling`
- `add-packaged-asset-bundles` (ARCHIVED) -> `add-game-project-build` packaging path

The single highest-leverage prerequisite is `aurora-renderer`: it gates the whole WS-WORLD workstream plus the
audio-world and navigation-integration attach paths. It is proposal-only, so making it actionable is the primary
unblocking action.

### Decision: Cleanup checklist (C1-C7; decisions recorded)

- **C1 — Park frozen/debt (decided): labeled "not scheduled", no archive.**
- **C2 — Proposal-only: `aurora-renderer` needs design/specs/tasks (owner: user).**
- **C3 — Dangling refs (done): 3 active ones reworded to "not yet a change"; `aurora-renderer` ref owner: user.**
- **C4 — Superseded rows (done): delivered rows marked; deferrals marked "no tracking change yet".**
- **C5 — Network (decided): `add-network-prediction` coexists with archived `network-lockstep`; prediction is the active model.**
- **C6 — Scene seam (decided): `aurora-renderer` owns `RenderSceneProxy`; `aurora-scene-bridge` transfers data only.**
- **C7 — Follow-ups (decided): no new tracking changes; missing items stay as "not yet a change" notes.**
- **C8 — Snapshot re-derivation (done): re-derived to the current 23 active changes after mid-flight archives and
  additions (see Context #5); `export-preset-schema-source` landed and archived cleanly.**

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
  executed, not a frozen truth (C8 re-derived it once already).
- [Unresolved `aurora-renderer` overlap blocks the largest workstream] -> Mitigation: make completing
  `aurora-renderer` and resolving C6 the top-priority unblocking tasks.

## Migration Plan

1. Record the snapshot (this change's artifacts).
2. For each checklist item, perform the operation separately with explicit confirmation (C1 requires user
   confirmation before any archive).
3. Re-derive the active list and confirm the end-state criterion holds. Rollback: no destructive operation is
   performed by this change, so reverting is a no-op.


## Aurora 状态校正（2026-10-17）

上文 23 项分类是历史快照。C2 的 design/specs/tasks 和 C3 的不存在材质 change 引用均已解决。aurora-renderer 的 Windows B01–B14/P01 已验收，待用户确认部分范围归档；aurora-renderer-layout 为 8/8。aurora-scene-bridge 现为 active，Windows 前置已解除；新增 aurora-renderer-platform-parity 为 blocked-on-prereq（Mac/原生设备），aurora-renderer-extensions 为 debt-record。新增记录仅为保留本次归档的实际余项，未将历史 C7 的其它设计缺口批量新建。

terrain/vegetation 的 CPU/render-feature 接缝可基于现有主线设计，但它们要求的 compute/indirect/完整材质仍未实现；不能把基础 Renderer 完成视为这些 feature 已可完整交付。navigation 的 Renderer 基础门已解除，仍需本 change 的适配设计。audio-world 的 World 提取前置应看 aurora-scene-bridge，而不是重新等待 Renderer 主循环。
