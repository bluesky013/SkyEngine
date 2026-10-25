## Current snapshot — 2026-10-26

This snapshot replaces the former 23-change table. The original 37-directory audit is extended by `remove-unused-legacy-rendergraph`, and the two extracted prerequisites `refactor-asset-product-transactions` and `fix-animation-sampling-validation`, for 40 active directories. Checked tasks do not imply semantic consistency, all-platform acceptance or archive approval. No change directory is moved, deleted or merged.

## Dispositions and workstreams

The fixed dispositions are active, blocked-on-prereq, frozen, debt-record and superseded. The workstream column assigns every current change; named prerequisites below are existing active changes. Platform/device availability is an acceptance gate, not a fictitious change dependency.

| Change | Disposition | Workstream | Checked/total | Remaining boundary |
|---|---|---|---|---|
| `add-android-python-runtime` | active | Platform/build | 14/20 | Retain unchecked implementation or acceptance gates. |
| `add-ios-platform` | active | Platform/build | 0/10 | Retain unchecked implementation or acceptance gates. |
| `add-ios-uikit-backend` | active | Platform/build | 8/13 | Retain unchecked implementation or acceptance gates. |
| `add-network-prediction` | active | Other foundation | 0/15 | Retain unchecked implementation or acceptance gates. |
| `add-python-ssl` | active | Platform/build | 15/20 | Retain unchecked implementation or acceptance gates. |
| `add-skeletal-animation-workflow` | active | Animation | 50/74 | Retain unchecked implementation or acceptance gates. |
| `animation-events` | active | Animation | 0/10 | Proposal has no delta; design work remains, not archive-ready. |
| `audio-animation-bridge` | blocked-on-prereq | Audio/Animation | 0/10 | Requires animation-events; proposal has no delta. |
| `audio-world-integration` | blocked-on-prereq | Audio/World | 0/8 | Requires aurora-scene-bridge completion; proposal has no delta. |
| `aurora-compute-custom-passes` | active | Aurora | 52/55 | Retain unchecked implementation or acceptance gates. |
| `aurora-cook-schema-layering` | debt-record | Aurora layering | 0/7 | Not scheduled; proposal has no delta and is not archive-ready. |
| `aurora-metal-bindless-descriptor-heap` | debt-record | RHI | 0/8 | Not scheduled; DirectArguments is the current path. |
| `aurora-navigation-integration` | active | Aurora | 0/8 | Proposal has no delta; design work remains, not archive-ready. |
| `aurora-renderer-extensions` | debt-record | Aurora extension | 0/5 | Not scheduled; retain unimplemented extension acceptance. |
| `aurora-renderer-platform-parity` | active | Aurora | 12/48 | Retain unchecked implementation or acceptance gates. |
| `aurora-scene-bridge` | active | Aurora | 15/16 | Retain unchecked implementation or acceptance gates. |
| `aurora-scene-lighting` | active | Aurora | 16/17 | Retain unchecked implementation or acceptance gates. |
| `aurora-world-editor` | active | Aurora | 9/9 | Tasks checked; archive still needs scope/merge review and user confirmation. |
| `consolidate-active-changes` | active | Other foundation | 12/12 | Tasks checked; archive still needs scope/merge review and user confirmation. |
| `fix-animation-sampling-validation` | active | Animation | 5/5 | Isolated algorithms passed 64 tests; archive still requires user confirmation. |
| `refactor-asset-product-transactions` | active | Framework assets | 5/6 | Isolated Framework passed 204 tests; native non-Windows filesystem acceptance remains open. |
| `fix-aurora-pass-resource-contract` | active | Aurora | 23/25 | Retain unchecked implementation or acceptance gates. |
| `fix-editor-skipped-frame-fence` | active | Editor | 8/9 | Retain unchecked implementation or acceptance gates. |
| `fix-editor-surface-gpu-teardown` | active | Editor | 8/9 | Retain unchecked implementation or acceptance gates. |
| `fix-sandbox-project-creation` | active | Editor | 10/10 | Tasks checked; archive still needs scope/merge review and user confirmation. |
| `lazy-transform-update` | frozen | Framework debt | 0/18 | Not scheduled; preserve the explicit freeze. |
| `navigation-terrain-build-tests` | active | Other foundation | 0/9 | Retain unchecked implementation or acceptance gates. |
| `refactor-aurora-framegraph-internals` | active | Aurora | 7/8 | Retain unchecked implementation or acceptance gates. |
| `remove-unused-legacy-rendergraph` | active | Aurora cleanup | 7/8 | Unused old graph/templates removed; fresh regression and native Metal gate recorded in that change. |
| `refactor-aurora-pipeline-resource-ownership` | active | Shader | 43/43 | Tasks checked; archive still needs scope/merge review and user confirmation. |
| `refactor-aurora-shader-source-techniques` | active | Shader | 22/23 | Retain unchecked implementation or acceptance gates. |
| `refactor-aurora-shared-pass-headers` | superseded | Shader | 8/8 | Current contract carried by pipeline-resource-ownership and target-shader-artifacts; old validation remains historical. |
| `refactor-aurora-slang-usage` | active | Aurora | 8/61 | Retain unchecked implementation or acceptance gates. |
| `refactor-aurora-target-shader-artifacts` | active | Shader | 44/50 | Retain unchecked implementation or acceptance gates. |
| `refactor-process-owned-reflection` | active | Other foundation | 13/13 | Tasks checked; archive still needs scope/merge review and user confirmation. |
| `sync-editor-surface-textures` | active | Editor | 10/11 | Retain unchecked implementation or acceptance gates. |
| `terrain-aurora-render` | active | Aurora | 0/18 | Retain unchecked implementation or acceptance gates. |
| `terrain-editor-tools` | blocked-on-prereq | Terrain | 0/7 | Requires terrain-aurora-render. |
| `vegetation-aurora-render` | active | Aurora | 0/12 | Retain unchecked implementation or acceptance gates. |
| `vegetation-editor-tools` | blocked-on-prereq | Vegetation | 0/5 | Requires vegetation-aurora-render. |

## Prerequisite order

- animation-events → audio-animation-bridge.
- aurora-scene-bridge → audio-world-integration.
- terrain-aurora-render → terrain-editor-tools; vegetation-aurora-render → vegetation-editor-tools.
- Shader contract order: shared-pass-headers → pipeline-resource-ownership → target-shader-artifacts → shader-source-techniques. Earlier active deltas must not overwrite later four-group, target ABI or typed-selection decisions.
- add-skeletal-animation-workflow consumes the current shader contracts and refactor-process-owned-reflection; check their shared files when constructing intermediate commits.
- Renderer foundation has already landed. Terrain, vegetation and navigation need their own implementation/capability checks; they must not wait for an unimplemented Renderer hub that no longer exists.

## Cleanup checklist

| Target | Disposition/action | Confirmation |
|---|---|---|
| Current shader and reflection deltas | Reconcile existing main requirements and preview merges; retain platform gates | No archive performed |
| refactor-process-owned-reflection, aurora-world-editor, fix-sandbox-project-creation | Eligible for explicit archive review after commit-boundary review | Required before archive and commit |
| refactor-aurora-shared-pass-headers, refactor-aurora-pipeline-resource-ownership | Review supersession and historical implementation scope; do not infer readiness from 8/8 or 43/43 | Required before archive |
| add-skeletal-animation-workflow and both current shader changes | Keep incomplete native/manual acceptance open | Not archive-ready |
| animation-events, audio-animation-bridge, audio-world-integration, aurora-cook-schema-layering, aurora-navigation-integration | No delta exists; retain as design/debt records, do not invent completed specs to silence validation | No archive proposed |
| Frozen/debt rows above | Keep explicitly not scheduled | Confirmation required to archive |

## Historical decisions retained

The earlier C1–C8 cleanup parked debt, repaired dangling planning references and clarified the scene seam. It did not archive directories. The 23-change count and Renderer-proposal-only prerequisite were historical snapshots, not the current backlog. Network prediction still coexists with the archived lockstep design; the selected prediction model is not reversed here.

## Acceptance boundary

Snapshot coverage, task counts, prerequisite names, delta mergeability and semantic/code evidence are separate checks. Recompute the table when the active set changes. An archive candidate still needs explicit user approval and a commit plan satisfying implementation-plus-spec ownership; a separate archive-only feature commit is not implied.
