---
name: openspec-change-hygiene
description: Keep the SkyEngine OpenSpec change backlog healthy by preventing the recurring debt seen in this repo - proposal-only changes, dangling references to nonexistent changes, superseded/stale task rows, frozen records mixed into scheduled work, and invisible prerequisite chains.
compatibility: claude
metadata:
  audience: contributors
  source: observed-backlog-debt
---

# OpenSpec Change Hygiene

Use this skill whenever you create, implement, or archive an OpenSpec change in this repository, or when the
user asks to "tidy up" / "consolidate" / "clean up" the change backlog. It encodes the failure patterns that
have actually accumulated here so they are not repeated.

## Why this exists

A live audit of `openspec/changes/` found 20 active changes with recurring, avoidable debt. None of it was
caused by a single mistake; each is a pattern that will recur unless checked at creation, implementation, and
archive time.

## Recurring failure patterns (observed in this repo)

For each: the symptom, a real example, how to prevent it, and how to fix it.

### 1. Proposal-only change (not actionable)

- **Symptom:** A change directory has `proposal.md` but no `tasks.md` (and often no `design.md`/`specs/`), yet
  it sits in the active list looking scheduled.
- **Example:** `aurora-renderer`, `legacy-render-thirdparty-cleanup`.
- **Prevent:** After `openspec new change`, run `openspec status --change <name>` and create every
  `applyRequires` artifact before leaving it. If it is intentionally a design stub, label it `debt-record` in
  the proposal and do not present it as ready.
- **Fix:** Generate the missing artifacts, or fold the content into the owning change, or mark/archive it.

### 2. Dangling reference to a nonexistent change

- **Symptom:** A proposal/design/tasks references a change name that exists neither active nor archived.
- **Examples:** `animation-graph-assets`, `aurora-animation-bridge`, `navigation-path-query`,
  `aurora-material-pso`.
- **Prevent:** Before writing a change name, confirm it with `openspec list` and `ls openspec/changes/archive`.
  Prefer naming an existing spec capability when no change exists.
- **Fix:** Repoint to a real change/spec, or create the follow-up change, or drop the reference.

### 3. Superseded task rows left behind after a sibling change is archived

- **Symptom:** A change still lists tasks that an already-archived change delivered, plus stale "deferred"
  notes.
- **Example:** `asset-pipeline` tasks 10.4/10.5/10.6c/14.4/14.5 were delivered by archived `asset-cook-ipc`.
- **Prevent:** On archive, grep dependent changes for the capability/topic you just shipped and trim or
  re-point their rows in the same pass.
- **Fix:** Mark superseded rows resolved/removed and remove the now-correct deferral notes.

### 4. Prerequisite already archived, dependent still listed as ordinary backlog

- **Symptom:** A change hard-depends on work that is already done, so its "blocked" state is invisible and it
  looks unscheduled for no reason.
- **Examples:** `add-network-prediction` on archived `add-network-core`/`add-network-replication`;
  `aurora-scene-bridge` on archived `harden-framework-components`.
- **Prevent:** When a change lists `depends on X`, and X is archived, immediately reclassify the dependent as
  ready and remove the stale gate.
- **Fix:** Update the dependent's status; it is now `active`, not backlog.

### 5. Frozen / debt records mixed into scheduled work

- **Symptom:** Explicitly frozen or "unimplemented debt" changes appear alongside scheduled ones with no
  distinguishing status.
- **Example:** `lazy-transform-update` ("Status: Frozen (not scheduled)"), `aurora-cook-schema-layering` and
  `aurora-metal-bindless-descriptor-heap` ("proposal (unimplemented)").
- **Prevent:** Put a status banner at the top of `proposal.md` AND `tasks.md` (`Frozen`, `Debt record`) and do
  not count them as scheduled.
- **Fix:** Keep as a labeled reference or archive after user confirmation (see `AGENTS.md`).

### 6. Deferral that cites an unnamed follow-up

- **Symptom:** "(deferred: sandbox editor refactor)" with no such change anywhere.
- **Example:** `asset-pipeline` and the terrain/vegetation editor-tools changes.
- **Prevent:** A deferral MUST name a real change or spec capability; if the follow-up does not exist, create a
  tracking change or state the concrete condition that unblocks it.
- **Fix:** Replace with a real name or a precise condition.

### 7. Overlapping changes that both design the same seam

- **Symptom:** Two changes add the same integration point with different names.
- **Example:** `aurora-renderer` (`RenderSceneProxy`) vs `aurora-scene-bridge` (`RenderSceneBridge`).
- **Prevent:** Before starting a change, grep the active set for the module/seam you are touching; reconcile
  ownership up front.
- **Fix:** Decide which change owns the seam; the other consumes it.

### 8. Ambiguous propose input causing repeated clarification

- **Symptom:** A vague prompt (e.g. "tidy up the state") forces several rounds of questions before work starts.
- **Prevent:** Treat the input as either a kebab-case change name or a concrete "build/fix X" description. If
  it is a goal ("clean up"), first resolve it to ONE concrete deliverable, then create the change.
- **Fix:** Ask at most one scoping question with concrete options, then proceed.

## Pre-flight checklist (before creating a change)

- [ ] Input is a change name or a concrete build/fix description; ambiguous goals resolved to one deliverable.
- [ ] Name is kebab-case and unique (`openspec list` shows no collision).
- [ ] Every `depends on` target is verified to exist (`openspec list` + `ls openspec/changes/archive`).
- [ ] You know which existing spec capability (or new one) it affects.
- [ ] You will create all `applyRequires` artifacts, not just `proposal.md`.

## Implementation-time checklist

- [ ] Keep `tasks.md` checkboxes accurate; mark done as you go.
- [ ] If a sibling/archived change already delivered a task, remove or re-point the row.
- [ ] Any new "deferred" note names a real change/spec or a concrete unblock condition.
- [ ] No reference to a change that does not exist.

## Archive-time checklist

- [ ] `openspec validate <name>` passes.
- [ ] Delta specs are synced (`openspec status` shows all artifacts done) - see `openspec-archive-change`.
- [ ] Grep the active set for dependents of what you are archiving; reclassify/repoint them.
- [ ] Trim superseded task rows in dependents in the same pass.
- [ ] Get explicit user confirmation before archiving (`AGENTS.md`).

## Periodic backlog audit

- [ ] `openspec list` - for each active change, assign one of: `active`, `blocked-on-prereq`, `frozen`,
      `debt-record`, `superseded`.
- [ ] Every `blocked-on-prereq` names a prerequisite that still exists.
- [ ] No `frozen`/`debt-record` change is presented as scheduled.
- [ ] No dangling change references anywhere in the active set.
- [ ] Overlapping seams resolved to a single owner.

## Verifying reference names

```bash
openspec list
ls openspec/changes
ls openspec/changes/archive
openspec status --change <name> --json
openspec validate <name>
```

## Anti-patterns (do not do)

- Hand-editing `openspec-*` skills - they are OpenSpec-generated; re-run `openspec init`/`openspec update`.
- Leaving a change with only `proposal.md`.
- Writing a `depends on`/`see change X` name without verifying X exists.
- Archiving without repointing dependents and trimming superseded rows.
- Mixing frozen/debt records into the scheduled list without a status banner.
- Turning a vague "clean up" request into a change before it is pinned to one deliverable.

## Output convention

When this skill is used for an audit/consolidation, report:

- The per-change disposition table (active / blocked-on-prereq / frozen / debt-record / superseded).
- The prerequisite graph (who gates whom).
- A cleanup checklist, each item with target, action, and whether user confirmation is required.
