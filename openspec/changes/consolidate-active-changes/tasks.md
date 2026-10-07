> Planning-first: this records the snapshot and cleanup decisions; the non-destructive cleanup has been applied
> with user approval. No change directory was archived (AGENTS.md). `aurora-renderer` (C2, C3) is owned by the user.

## 1. Snapshot

- [x] 1.1 Record dispositions, workstream grouping, prerequisite graph, and cleanup checklist C1-C7 (see `design.md`)
- [x] 1.2 Verify no dangling reference remains in this change's artifacts

## 2. Unblock the aurora render hub

- [ ] 2.1 Complete `aurora-renderer`: add `design.md`/`specs/`/`tasks.md` (C2) - owner: user
- [x] 2.2 Scene-seam decision recorded: `aurora-renderer` owns the seam; `aurora-scene-bridge` transfers data only (C6)
- [x] 2.3 Missing-spec changes left as-is; no new tracking changes (C7)

## 3. Park frozen / debt-record changes

- [x] 3.1 Keep `lazy-transform-update` (frozen) and the three debt-records as labeled "not scheduled"; no archive (C1)

## 4. Fix references and trim superseded content

- [x] 4.1 Reword 3 dangling refs to "not yet a change": `animation-events`, `audio-animation-bridge`, `aurora-navigation-integration` (C3)
- [ ] 4.2 Resolve `aurora-renderer` ref to nonexistent `aurora-material-pso` (C3) - owner: user
- [x] 4.3 Mark `asset-pipeline` rows 10.4/10.5/10.6c/14.4/14.5 delivered by archived `asset-cook-ipc`; deferrals marked "(no tracking change yet)" (C4)

## 5. Reconcile network prediction

- [x] 5.1 `add-network-prediction` coexists with archived `network-lockstep`; prediction is the active model (C5)

## 6. Verify the end state

- [x] 6.1 Active set re-checked: all `active`/`blocked-on-prereq`; only residual dangling ref is `aurora-renderer` -> `aurora-material-pso` (owner: user)
