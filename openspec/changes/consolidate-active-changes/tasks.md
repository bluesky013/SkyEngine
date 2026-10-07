> This change is planning-only: it records the snapshot and the cleanup decisions. The non-destructive cleanup
> (C3 reference rewording, C4 superseded-row trimming, C1 parking labels, C5/C6/C7 decision records) has been
> applied with user approval. NO change directory was archived; archiving remains a separate, user-confirmed
> action (AGENTS.md). `aurora-renderer` (C2, C3) is owned by the user.

## 1. Record the consolidation snapshot

- [x] 1.1 Record the disposition of all 20 active changes per the design classification table
- [x] 1.2 Record the workstream grouping (WS-PYTHON, WS-AURORA, WS-WORLD, WS-AUDIO, WS-NET, WS-ASSET, WS-RHI, WS-DEBT)
- [x] 1.3 Record the prerequisite graph, including `aurora-renderer` as the WS-WORLD gate
- [x] 1.4 Record the cleanup checklist items C1-C7 with target change, action, and confirmation flag
- [x] 1.5 Verify no dangling reference remains in this change's artifacts

## 2. Unblock the aurora render hub

- [ ] 2.1 Complete `aurora-renderer`: add `design.md`, `specs/`, and `tasks.md` so it is actionable (C2) - owner: user
- [x] 2.2 Resolve the `aurora-renderer` (`RenderSceneProxy`) vs `aurora-scene-bridge` (`RenderSceneBridge`) scene-seam overlap; decision recorded: `aurora-renderer` owns the seam (C6)
- [x] 2.3 Missing-spec changes decision recorded: leave `aurora-navigation-integration` / `audio-world-integration` as-is; no new tracking changes (C7)

## 3. Park frozen and debt-record changes

- [x] 3.1 `lazy-transform-update` (frozen): decision = keep as labeled reference; not scheduled; no archive
- [x] 3.2 `aurora-cook-schema-layering` (debt-record): banner updated to "Debt record - not scheduled"
- [x] 3.3 `aurora-metal-bindless-descriptor-heap` (debt-record): banner updated to "Debt record - not scheduled"
- [x] 3.4 `legacy-render-thirdparty-cleanup` (debt-record): banner updated to "Debt record - not scheduled"
- [x] 3.5 No archive performed; all four remain in place with explicit not-scheduled status

## 4. Fix references and trim superseded content

- [x] 4.1 Resolved `animation-events` reference: reworded to "Animation graph assets (not yet a change)"
- [x] 4.2 Resolved `audio-animation-bridge` reference: reworded to "Aurora animation bridge (not yet a change)"
- [x] 4.3 Resolved `aurora-navigation-integration` reference: reworded to "Navigation path query (not yet a change)"
- [ ] 4.4 Resolve `aurora-renderer` reference to nonexistent `aurora-material-pso` (C3) - owner: user (handling `aurora-renderer` directly)
- [x] 4.5 Trimmed `asset-pipeline` superseded task rows 10.4/10.5/10.6c/14.4/14.5, marked delivered by archived `asset-cook-ipc` (C4)
- [x] 4.6 Asset-pipeline "sandbox editor refactor" deferrals marked "(no tracking change yet)"; no tracking change created (C4)

## 5. Reconcile network prediction

- [x] 5.1 Decision recorded: `add-network-prediction` coexists with archived `network-lockstep`; prediction is the active model (C5)

## 6. Verify the end state

- [x] 6.1 Active set re-checked: every non-archived change is `active` or `blocked-on-prereq`; the only residual dangling reference is `aurora-renderer` -> `aurora-material-pso`, owned by the user
- [x] 6.2 All `frozen`/`debt-record` changes carry an explicit not-scheduled banner; end-state criterion holds apart from the user-owned `aurora-renderer` item
