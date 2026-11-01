# Verification: process-resident module code

Date: 2026-10-30. Windows / Visual Studio 2022 / Debug, existing `cmake-build-debug` cache and third-party path preserved.

## Builds

- Full engine Debug build passed: `cmake --build cmake-build-debug --config Debug -- /m:1 /nr:false`.
- Real Core/Framework, Aurora runtime/adaptor/editor, Vulkan/DX12, shader compiler, Content, animation and Jolt libraries/modules were rebuilt against the shared Environment definition. No target/source ownership restructuring was added.
- External PartySoftBody final build passed for `PartySoftBodyModule`, `PartySoftBodyTools` and `Launcher` in its existing Windows build tree.
- Build logs: `build/module-code-cleanup-review/{resident-build,full-build,party-final-build}.log`. Existing third-party missing-PDB warnings remain; no build errors.

## C++ regressions

The latest XML results under `build/module-code-cleanup-review/` contain **1335 passed, 0 failed, 4 skipped and 5 disabled** cases. Repeated runs are counted once per final XML file.

| Executable / selection | Passed | Skipped / disabled |
| --- | ---: | --- |
| CoreTest | 314 | 0 |
| FrameworkTest | 211 | 0 |
| Aurora.CoreTest | 107 | 0 |
| EditorCoreTest | 184 | 0 |
| EditorShellTest | 144 | 0 |
| EditorRenderTest | 53 | 0 |
| SandboxProjectTest | 15 | 0 |
| Content.AdaptorTest | 7 | 0 |
| Animation.WorldTest | 10 | 0 |
| Animation.EditorTest | 8 | 0 |
| Animation.ModuleTest | 2 | 0 |
| JoltPhysicsTest | 58 | 5 pre-existing disabled |
| Aurora.AdaptorTest: lifecycle, RendererHost, RenderSceneProxy, LauncherSurface, UI and skeleton source | 19 | 0 |
| Aurora.Renderer.ModuleTest | 8 | 2 |
| Aurora.Renderer.StaticModuleTest | 9 | 2 |
| Aurora.Renderer.GpuSubmissionTest: Feature registry, RendererRuntime, editor/native hosts and Vulkan device config | 186 | 0 |

The four skipped cases are the two shipped-product-only scenarios in each module fixture; the packaged shader-product fixture directory is unavailable. PhysicsTest built and exited successfully but currently contains zero test cases; physics runtime coverage comes from JoltPhysicsTest.

Focused new/updated assertions verify:

- Shared singleton removal/destruction followed by fresh construction, nested construction and 16-thread first lookup returning one object.
- Failed Init cleanup and dependent rejection; metadata registration remains once across failed Init, retry and manager replacement.
- Cold instances receive no service callbacks. Init rejects nested loading/teardown and cannot start/tick partially assembled providers.
- Shutdown removes readiness first and follows `consumer Shutdown -> consumer destructor -> provider Shutdown -> provider destructor`. Repeated teardown is harmless.
- Real Aurora/ShaderCompiler DLL handles remain loaded after sessions stop. Source interfaces unregister while owned requests remain usable.
- Async futures retain actual compiler/writer objects; Feature frames, preview Worlds, documents, cursor/geometry and undo data retain real owners through their existing retirement boundaries.

An initial animation test incorrectly required valid view data for a random missing asset; it now verifies an invalid result. Both animation module tests pass after the correction. Core, Framework and dynamic/static module tests were rerun after the final shutdown-order change. The 186 GPU cases exercise unchanged rendering/retirement behavior after code-token removal; the later adjustment only changes ModuleManager session teardown.

## External smoke checks

Only the two Feature registration callers were adapted:

- `F:/Investigate/PartySoftBody/native/source/src/DebugOverlay.cpp`
- `F:/Investigate/PartySoftBody/native/source/tools/PartySoftBodyTools.cpp`

Bounded runs of the rebuilt executable passed, including native renderer shutdown:

- World1: 120 ticks, exit 0, one contact/rebound.
- World2 (default penguin): 600 ticks, exit 0, one contact/rebound; contact at tick 122 and confirmed separation at tick 146. Error logs were checked in addition to the process exit status.

The initial World2 120-tick horizon stopped before its first contact, so it was extended to 600 ticks. These smoke checks validate startup, default model contact and shutdown; they do not claim coverage of all model switches, wall/drop scenarios or parameter tuning.

## Static checks and review

- Source ownership Python suite: 26 passed. Its first sandbox run could not create a temporary directory; the normal-process rerun passed.
- clang-format dry run: all 98 changed C++ headers/sources passed.
- No added throw/catch/dynamic_cast paths. Repository-wide C++ scan found no `RetainModule`, `RetainModuleCode`, `moduleLease`, `codeLease` or `RequiresModuleLease` remnants.
- Independent reviews confirmed shared lookup/session-state correctness and retained Feature/document/provider/preview/GPU ownership.
- Root/Aurora rules and World/shader guides now describe process residency. Historical records and unrelated dirty/untracked design documents were not rewritten.
- OpenSpec strict validation and scoped whitespace checks passed.

## Commit boundary

The prior Vulkan configuration change is committed as `40f70051`. This cleanup remains an independent, reviewable working-tree change; the proposed message and exact file list are in `commit-scope.md`. No OpenSpec archive was performed.

Existing staged entries were preserved exactly: `docs/README.md` (`f62e51cf0a48021f4a6e9baa56185e281090848a`) and `engine/framework/test/assets/assets.db` (`1bdedaa8944fd1df628b3cedfc4eadd82a012831`). Neither file, unrelated documentation/spec changes nor external project files belong to the proposed engine commit.

## 2026-11-01 strict review and fresh validation

The earlier October 30 results above remain historical. This follow-up synchronized
51 active/reference documents (46 tracked contracts and five already-untracked
research documents), added 11 primary-capability deltas, and reviewed the complete
Core/Framework, Aurora, Content, Animation, Sandbox and Jolt lifetime cleanup.

### Reproduced issue and repair

The original Init/Shutdown-only guards allowed Start/PreTick/Tick/PostTick callbacks
to recursively dispatch and destroy their own executing module via UnLoadModules.
The new regression failed before the fix, observing repeated callbacks and early
destruction. One manager phase now guards dispatch and returns explicit failure for
reentry and registration/topology mutation. StartModules and Tick also return status;
Init, Shutdown and all four service-phase tests verify these rejection results.
Normal host-thread teardown remains available after the dispatch returns.

### Final build and regressions

- Full engine Debug build passed with the existing VS 2022 cache and third-party path:
  `cmake --build cmake-build-debug --config Debug -- /m:1 /nr:false`.
- This rebuilt the actual libraries, modules and test executables; no target/source
  ownership changes were introduced. The initial final log is `full-build-final.log`;
  the last explicit-status refinement is rebuilt in `explicit-status-build.log`.
- Fresh final XML results: **1336 passed, 0 failed, 4 skipped and 5 disabled**.

| Executable / selection | Passed | Skipped | Disabled |
| --- | ---: | ---: | ---: |
| CoreTest | 314 | 0 | 0 |
| FrameworkTest | 212 | 0 | 0 |
| Aurora.CoreTest | 107 | 0 | 0 |
| EditorCoreTest | 184 | 0 | 0 |
| EditorShellTest | 144 | 0 | 0 |
| EditorRenderTest | 53 | 0 | 0 |
| SandboxProjectTest | 15 | 0 | 0 |
| Content.AdaptorTest | 7 | 0 | 0 |
| Animation.WorldTest | 10 | 0 | 0 |
| Animation.EditorTest | 8 | 0 | 0 |
| Animation.ModuleTest | 2 | 0 | 0 |
| JoltPhysicsTest | 58 | 0 | 5 |
| PhysicsTest | 0 | 0 | 0 |
| Aurora.AdaptorTest | 19 | 0 | 0 |
| Aurora.Renderer.ModuleTest | 8 | 2 | 0 |
| Aurora.Renderer.StaticModuleTest | 9 | 2 | 0 |
| GpuOwnership | 186 | 0 | 0 |

GpuOwnership selects FrameFeatureRegistry, RendererRuntime, EditorHostNative,
RendererHostNative and VulkanDeviceConfig in Aurora.Renderer.GpuSubmissionTest.
The four skips remain the missing shipped shader-product fixtures in the dynamic
and static module suites. Five Jolt benchmark cases remain explicitly disabled.
PhysicsTest exits successfully with zero cases; JoltPhysicsTest supplies runtime
physics coverage. These counts use one final XML per executable/selection, not
the expected pre-fix failure or a stale-binary intermediate attempt.

After the final explicit-status change, the complete engine was rebuilt again and
all 17 executable selections above were rerun from those binaries, including the
186 native GPU cases. Their XML timestamps are later than the final build log.
The intermediate results are preserved in results-before-status and are not added
to the final totals.

The sandboxed MSVC attempt reported C1902/PDB-manager errors. The native retry
exposed four stale Framework object files retaining old method signatures; only
those verified Debug objects were removed before the successful complete build.
No source workaround or cache/generator/third-party-path change was needed.

### Static checks and preserved work

- CMake source-ownership suite: 26 passed in the normal process environment.
- clang-format dry-run with errors enabled: all 98 changed C++ files passed.
- OpenSpec strict validation: 22 passed (11 active changes and 11 primary specs).
- Prepared commit patch, whitespace and unrelated-work preservation are checked
  separately from the existing index; the exact scope is in commit-scope.md.
- New findings and their root causes are in review.md. No unresolved blocking
  finding remains in the reviewed scope after this fix and these regressions.

Evidence for this follow-up is under
`build/module-code-cleanup-review-20261101/`, including `repro.{log,xml}`,
`results/{summary.json,*.xml,*.log}`, `spec-validation.json`,
`source-ownership-native.log`, `format-check.log`, `commit.patch` and
`preservation.json`. Build/test artifacts are excluded from the commit.

This run covers Windows Debug. External PartySoftBody was not rebuilt/rerun in
this follow-up; its October 30 API adaptation/smoke results above are historical.
Linux, macOS, mobile and Release behavior/performance are not newly validated.
No commit, staging change or OpenSpec archive is performed before confirmation.

## 2026-11-01 rebase onto origin 741a3371

The three local commits were replayed onto `origin/dev_refactor_rhi` at `741a3371`. Conflict resolution preserves upstream Vulkan 1.1/KHR negotiation and presentation support, owned launcher sessions and retirement/reentrant shutdown, simplified asset storage, and per-native-image Jolt SDK state. New navigation, compressor, scene session and editor overlay consumers no longer pass pure code leases. The original approval and its 167-file patch remain recorded; the final lifecycle commit includes integration fixes listed in commit-scope.md.

Validation of the merged sources:

- Existing VS 2022 Debug cache configured successfully; the full Debug build passed after adapting three stale upstream API call sites.
- 1505 tests passed, 0 failed, 7 skipped and 5 explicitly disabled across 23 executable selections. All final XML timestamps follow the successful full build.
- The native selection ran 186 cases, with 185 passing and one backend-specific skip: Feature retirement, renderer runtime, launcher/editor lifecycle and all four Vulkan device preference/support/entry-point combinations.
- Core/Framework, animation, editor, Jolt, the 16 Vulkan capability probes and navigation runtime/module/render/editor suites passed. New scene-source precedence and overlay retirement ownership checks passed.
- Source-ownership checks: 26 passed. Changed C++ clang-format checks: 149 passed. Affected OpenSpec strict checks: 29 passed. The all-repository validator also reported 10 unchanged, unrelated invalid items; those were not modified here.

The seven skips are one Windows file-symlink privilege check, four missing shipped shader-product fixtures, one navigation scene requiring `SKY_NAVIGATION_SCENE_PROJECT` and the DX12 instance of the Vulkan surface-transform probe. Five Jolt benchmarks remain disabled. PhysicsTest contains zero cases; JoltPhysicsTest supplies the physics coverage.

The first 189-case native selection was stopped after roughly four minutes in `EditorHostNativeTest.StaticPickingUsesProductionSceneAndSharedSelection/0`. A live symbolic thread snapshot showed the host executing Slang Debug `GenericArgumentSolver`/semantic analysis. The shader sources and compiler implementation have no local diff against the fetched origin; this identifies the observed long-running stage, not a proof of its deeper cause. The subsequent selection excludes the three backend instances of this scenario, so their validation is **incomplete** and is not included in the passing totals. The interrupted attempt is retained separately and is not counted as a successful suite.

Evidence: `build/module-code-cleanup-review-20261101/rebase-{configure,build-final,format}.log`, `rebase-source-ownership.log`, `rebase-openspec-selected.json`, `rebase-results/`, `gpu-threads.log` and `rebase-final-checks.json`. This verifies Windows Debug only; external PartySoftBody, Linux, macOS, mobile and Release were not rebuilt or rerun in this rebase. No push or archive is part of the request.
