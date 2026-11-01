> Current pass (2026-11-01): sections 1–6 are historical; section 7 records the domain audit, section 8 the explicitly approved archive/backlog execution. Commit/push follow the separate AGENTS confirmation rule.

> Historical pass: C2/C3 were resolved by aurora-renderer through B14. Current state is maintained in design.md; the original dated snapshots remain in Git history.

## 1. Snapshot

- [x] 1.1 Record dispositions, workstream grouping, prerequisite graph, and cleanup checklist C1-C7 (see `design.md`)
- [x] 1.2 Verify no dangling reference remains in this change's artifacts
- [x] 1.3 Re-derive the snapshot to all 37 current non-archive changes (2026-10-26); record task counts, dispositions, real prerequisites and archive boundaries in design.md.

## 2. Unblock the aurora render hub

- [x] 2.1 Complete `aurora-renderer`: add `design.md`/`specs/`/`tasks.md` (C2) - owner: user
- [x] 2.2 Scene-seam decision recorded: `aurora-renderer` owns the seam; `aurora-scene-bridge` transfers data only (C6)
- [x] 2.3 Missing-spec changes left as-is; no new tracking changes (C7)

## 3. Park frozen / debt-record changes

- [x] 3.1 Keep `lazy-transform-update` (frozen) and the three debt-records as labeled "not scheduled"; no archive (C1)

## 4. Fix references and trim superseded content

- [x] 4.1 Reword 3 dangling refs to "not yet a change": `animation-events`, `audio-animation-bridge`, `aurora-navigation-integration` (C3)
- [x] 4.2 Resolve `aurora-renderer` ref to nonexistent `aurora-material-pso` (C3) - owner: user
- [x] 4.3 Mark `asset-pipeline` rows 10.4/10.5/10.6c/14.4/14.5 delivered by archived `asset-cook-ipc`; deferrals marked "(no tracking change yet)" (C4)

## 5. Reconcile network prediction

- [x] 5.1 `add-network-prediction` coexists with archived `network-lockstep`; prediction is the active model (C5)

## 6. Verify the end state

- [x] 6.1 Active set re-checked: all `active`/`blocked-on-prereq`; the historical aurora-material-pso reference is now explicitly identified as a non-goal without an existing change


## 7. Domain consolidation — 2026-11-01

- [x] 7.1 Rebase onto latest origin and re-audit all 55 active changes against that baseline.
- [x] 7.2 Consolidate 176 main capabilities into 32 domains and preserve all 1,327 requirement bodies with an explicit migration map.
- [x] 7.3 Migrate all active delta paths, proposal capability IDs and concrete references without rewriting historical archives.
- [x] 7.4 Separate actual implementation, remaining acceptance, transferred/historical work, planned items and frozen/debt records; remove obsolete blockers.
- [x] 7.5 Complete current global validation and the exact archive synchronization preview.
- [x] 7.6 Record the final verification evidence and leave the archive/commit confirmation gate explicit.

## 8. Approved archive and backlog execution — 2026-11-01

- [x] 8.1 Apply the reviewed 65 additions, 2 removals, 3 full updates and 1 scenario-only merge with source/current/result hashes; retain all 21 protected canonical bodies.
- [x] 8.2 Archive the approved 24 changes with CLI spec synchronization skipped after the selected sync, retaining transferred P02 acceptance.
- [x] 8.3 Park the four undesigned proposals under backlog; preserve all 11 original files and incomplete tasks.
- [x] 8.4 Update current indexes, task/owner counts and path references without changing historical archives or the residency commit manifest.
- [x] 8.5 Verify 32 domains / 1,390 main requirements, 27 active changes, 4 backlog records, strict validation and historical file hashes; record the actual result without commit/push.
