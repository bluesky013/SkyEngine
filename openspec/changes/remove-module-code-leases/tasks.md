## Implementation

- [x] Audit code leases versus real data/SDK/task/GPU ownership and establish the process-resident DLL contract.
- [x] Implement the Core native library owner and explicit Framework session readiness/metadata/shutdown state.
- [x] Remove pure code fields and parameters across Framework, animation/content, renderer/editor, shader and physics while preserving real ownership.
- [x] Migrate lifecycle tests and adapt the two external PartySoftBody Feature registrations.
- [x] Build affected real targets and run Framework/module/shader/animation/Jolt/editor/native regressions and source-ownership checks.
- [x] Document verification, source review and the exact engine commit scope; preserve unrelated staged/user changes and do not archive.

## 2026-11-01 review follow-up

- [x] Synchronize active code-lifetime documents and primary specifications; label historical evidence without rewriting its recorded results.
- [x] Reproduce callback self-destruction and recursive dispatch, then reject lifecycle and registration reentry through one manager phase.
- [x] Rebuild the real affected modules and rerun lifecycle, ownership, native renderer and static checks after the final explicit-status refinement.
- [x] Record final findings, exact staged exclusions and a reviewable commit patch for user confirmation; user confirmed this scope and the commit was created before the requested rebase.

## Requested rebase integration

- [x] Rebase all three local commits onto fetched origin `741a3371`, preserving new runtime/storage/Vulkan/Jolt behavior.
- [x] Migrate new navigation, compressor, launcher session and overlay code-lease consumers with object-ownership regressions.
- [x] Build the full Debug configuration and run affected C++/native/source-ownership/format/OpenSpec checks; record exact exclusions and limitations.
- [ ] Complete the three production-picking native cases after investigating the long-running Slang Debug semantic-analysis stage. This rebase does not claim full native-suite acceptance.
