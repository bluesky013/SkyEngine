# DLL lifecycle review — 2026-11-01

Scope: the process-resident DLL change on `40f70051`, including Core/Framework,
Aurora, Content, Animation, Sandbox and Jolt consumers. Historical archive moves,
unrelated research documents and the pre-existing index entries are separate work.

## Findings and root causes

1. **P1 — service callbacks could destroy their executing module. Fixed.**
   `loading`/`stopping` guarded Init and Shutdown but did not cover Start or the three
   Tick phases. A callback could recursively dispatch, or unload the manager and
   destroy its own instance before returning. Registration/topology mutation could
   also invalidate containers under traversal. The new regression reproduced early
   destruction and repeated dispatch before the fix. A single lifecycle phase now
   returns explicit failure for nested transitions and structural mutation; normal teardown remains
   available after dispatch. Tests cover all four service phases plus Init/Shutdown.

2. **P2 — active documentation contradicted the loader contract. Fixed.**
   The original cleanup changed implementation and selected guides, but left primary
   specs and dependent active designs requiring code tokens and final DLL unload.
   Fifty-one documents are synchronized or explicitly marked as historical evidence.
   Eleven capability deltas capture primary-spec changes in this change. Compiler,
   SDK, World, Feature, document, cursor, asset and GPU ownership remains explicit.

## Ownership audit

- ResidentLibraries shares successful native handles through Environment. Loading
  occurs outside its registry mutex; failed loads are not cached. Concurrent first
  loads may retain additional native references until process exit by design.
- Singleton lookup no longer caches a destroyed object's address inside each DLL.
  Shared construction is serialized and nested singleton lookup is supported.
  Destruction still requires callers to drain users; this is not concurrent deletion.
- Process metadata definitions remain distinct from manager sessions. Failed Init
  cleans the partial instance, dependencies use current-manager readiness, and
  shutdown removes readiness before callback/destruction in reverse dependency order.
- Shader futures keep compiler/cache owners; animation requests keep operation and
  product owners; physics shapes/worlds keep SDK state. Removing code tokens does
  not remove these owners or authorize work against a stopped provider.
- FrameFeatureLease, GPU retirement, preview World ownership, provider/cursor data,
  document history and stale-action generation checks remain in place.
- ModuleManager is host-thread-only. Binary replacement/removal requires restart;
  process residency does not make live services available or support hot reload.

## Validation

Before the requested rebase, the full Windows Debug build passed. That checkpoint reported 1336 passed,
0 failed, 4 skipped and 5 disabled; details and platform limits are in
verification.md. No unresolved blocking finding remains in this reviewed scope. The
pre-fix failure is retained separately in
`build/module-code-cleanup-review-20261101/repro.{log,xml}`; it is not part of final
pass counts. Windows Debug validation does not establish Linux/macOS/mobile runtime
acceptance or Release performance.

## Commit boundary

Implementation, this change and synchronized tracked contracts belong together.
For already-dirty documentation, the prepared patch includes only this contract
update. Two requirement blocks already present in the working-tree renderer/shader
specs are included because their ownership requirements are directly affected;
the remaining pending archive/spec changes stay out.

Five previously untracked research documents are updated in the working tree but
excluded as whole new research artifacts: softbody-integration-plan.md,
weather-system-design.md, flight-simulation-integration-plan.md and
pcg-technology-and-plugin-gap-analysis.md, plus pcg-plugin-detailed-design.md. Existing staged docs/README.md and
engine/framework/test/assets/assets.db remain unchanged in the index. No commit or
archive is performed before user confirmation.

## Requested rebase integration

Upstream introduced new code-lease consumers, replaced ProductGroup storage APIs, added component schema queries and corrected Jolt SDK initialization to follow each native static image. Blindly choosing either conflict side would remove upstream functionality or reintroduce APIs deleted by this change. The resolution retains those upstream contracts and removes only code-lifetime plumbing; launcher session, overlay, compressor, navigation task and SDK object ownership remains explicit.

The full build exposed three further stale aggregate/API calls in newly added renderer/World tests and the overlay registry. These are migrated with their callers. New tests prove project-versus-default scene precedence, explicit-session precedence and overlay lifetime through retirement. Verification results and the incomplete three-backend production-picking scenario are recorded in verification.md; no successful result is claimed for that scenario.
