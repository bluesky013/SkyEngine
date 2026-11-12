# Preparation evidence

## 2026-11-01

- User confirmed at most four influences per visible vertex, with a larger control-point set permitted.
- User selected the seal example.
- `openspec validate add-jolt-softbody-generators --strict`: passed.
- User approved the corrected D1 interface ownership: engine/physics/softbody with an independent Physics-domain target.
- Fetched origin and rebased dev_refactor_rhi; both already pointed to 6030930720ce8a56533547496a8f20590571905e. Restored and verified local staged/unstaged patches, file contents, original line endings and untracked files. Existing stashes were preserved; an additional recovery stash and external backup remain.

## Original unbound seal

Artifact directory: `C:/Users/bluesky/.codex/visualizations/2026/11/01/01a19720-4b10-7872-85c9-e32d17581722/seal-softbody-prototype`.

- `seal-body.glb`: single closed body; 20,000 triangles; 10,002 welded vertices (10,078 exported vertices including UV seams).
- `seal-unrigged.glb`: complete animal, 29,720 triangles, 25 meshes, 8 materials; body plus 24 face accessories.
- glTF coordinates are meters, Y-up, forward +X; body length is approximately 1.0208 m.
- Source body and exact-position-welded exported body: one connected component, zero boundary edges, non-manifold edges, inconsistent winding edges and degenerate triangles; positive volume approximately 0.06590 cubic meters.
- Actual Blender mesh previews `seal-preview.png` and `seal-front.png` were inspected; the final 3/4 preview is the current visual reference.
- `create_seal.py`, `inspect_glb.py`, `topology-audit.json`, `export-audit.json` and `asset-notes.json` preserve reproduction and inspection evidence.

The GLBs contain no skins, armatures or animations. They have not been imported into SkyEngine or accepted as a Jolt/automatic-binding result. Face accessories require an explicit attachment policy during production generation. The prototype must be passed through the real generator for final acceptance.

## Cage feasibility experiment

The separate cage-feasibility prototype tested nine voxel resolutions and 40 grid alignments. A 254-point/799-tet candidate preserved two exact mandatory points, covered all 10,078 vertices with at most four nonnegative weights, and reproduced rest positions to 1.27e-16 m. Its volume exceeded the source by 166.6%, and its sampled maximum external gap was 17.65 cm. Binding mathematics passed; collision-proxy fidelity failed. This is not production generator or native Jolt acceptance evidence. Detailed data and reproducible scripts are in the prototype directory's cage-feasibility subdirectory.

## 2026-11-02 implementation checks

The user superseded the earlier 255-point example with a 30-point example, then explicitly requested a configurable count. Production authoring therefore defaults to a 30-point budget and persists the selected budget; it does not hardcode 30 as its maximum. Default controls are all dynamic with finite-compliance XPBD root following. The original XPBD rabbit uses 15 positive-inverse-mass points and compliant following; the force-based SpringRabbit is a separate implementation and is not the selected target.

- Actual C++ Authoring seal fixture: 30 controls, 56 tetrahedra, volume excess 0.744920, certified cage-to-display distance upper bound 0.159956 m, minimum tet quality 0.0217469, maximum rest reconstruction error 6.79853e-8 m. The distance is one-directional; the collider is deliberately coarse. Debug generation took approximately 2.1 s. All 6 Authoring tests passed.
- Actual complete `seal-star-profile.glb` passed production import, GenerateRig, Mesh/Skin/Skeleton/SoftBody cook, product reload, repeat stable identity and failed-compiler preservation checks. Both Builder integration tests passed; seal test took approximately 6.2 s including repeated builds.
- SoftBody.AssetsTest: 8 passed before the final configurable-budget correction, including recipe enum boundaries, finite follow compliance and product roundtrip; the revised capacity/persistence test awaits rerun.
- SoftBody.EditorTest: 2 passed, exercising the actual registered provider and recipe codec.
- EditorCoreTest: all 196 passed, including 9 generator registry and 3 generated-source persistence checks.
- Aurora.CookTest: all 54 passed, including authoring compiler and zero-weight slot canonicalization.
- Python CMake source ownership: 26 passed. Third-party patch-key tests: 2 passed.
- Actual Authoring, Assets, Builder, Editor and Aurora.Render.Editor implementation targets built successfully. Mesh document lifecycle tests initially failed because DLL environments require the normal module attach path; investigation is in progress.

Native Jolt point-follow rebuild/tests and the final editor/runtime scene are still pending at this checkpoint. Earlier native softbody tests do not by themselves verify the newly requested root-follow patch.

### Later checkpoint: native follow and host integration

This and the following native checkpoint are historical. Their native softbody and patched-Jolt results were superseded by the independent CPU route recorded below; they are not evidence for the final solver implementation.

- Rebuilt and installed Debug and Release Jolt from the checked pinned source with the point-follow patch. Patch-aware package key: `4bc377da39578e9a`. The Jolt dependency precheck passes; the full dependency precheck still reports eight unrelated stale/missing packages.
- Current native backend: all 81 JoltPhysicsTest tests passed, with 5 existing disabled tests. The focused set passed 13 softbody and 6 fixed-step-listener tests, including analytic follow compliance/mass, moving-target collision bounds, rigid reaction, COM movement/reset, sleep/wake and invalid-input preservation.
- The 30/60/120 Hz comparison uses the same fixed-step input sequence. It does not establish equivalence for arbitrary actor motion sampled independently at render frequency.
- Current SoftBody.WorldTest: 8 passed before the additional all-dynamic root-follow component regression. That regression and the final configurable-budget asset test are being rebuilt.
- Authoring: all 7 tests passed, including different configurable budgets above and below the 30-point default.
- EditorShellTest: all 153 passed, including 5 Mesh document lifecycle cases. The earlier test setup issue was fixed by loading the real metadata owner through ModuleManager and its normal DLL attachment path.
- Actual JoltPhysicsModule, SoftBody.World, SandboxModule, SandboxEditor and AssetTool builds completed successfully. Final incremental builds also include the real AssetTool softbody builder entry and the reflected-form lifetime correction.
- The prepared seal project is in the sibling `seal-softbody-demo` artifact directory. A zero exit code from the first short editor invocation is not visual acceptance evidence; real project cook and editor/runtime inspection remain pending.

### Final incremental checks before mobile backend review

- The final configure and actual affected module/AssetTool/SandboxEditor builds passed.
- SoftBody.AssetsTest: 8 passed, including persistence of a 48-control budget and rejection above renderer capacity.
- SoftBody.WorldTest: 9 passed, including all-dynamic root following with no kinematic control. The observed point displacement/velocity matches the finite-compliance analytic result.
- MeshDocumentTest: all 5 passed after the reflected settings-address and drag/history fixes. An initial incorrectly named gtest filter selected zero tests; it was corrected and is not counted as a pass.
- CMakeSourceOwnership: 26 passed; third-party patch-key checks: 2 passed. Tests use the workspace temporary directory; the sandbox's default temporary directory produced access failures before any meaningful test execution.
- OpenSpec strict validation passed. `git diff --check` reports unified-diff context lines in `cmake/patches/jolt.patch` (space prefixes before native tabs/empty context), so a clean global whitespace result is not claimed.
- The new mobile platforms and possible PhysX backend are requirements, not tested support. No Android, iOS or OHOS build, device deployment or mobile performance acceptance was performed in this checkpoint. No solver replacement was made during the PhysX research.

## Final independent CPU route verification

The user approved replacing the experimental native Jolt softbody implementation with `SoftBody.Solver.Static`, an independent CPU XPBD solver consuming `Physics.SoftBody.Interface`. Jolt now provides rigid-world collision queries; it does not integrate the control particles. The native point-follow patch and experimental native softbody path were removed, and the original Jolt SDK patch set was rebuilt in Debug and Release. The earlier native-follow results above are retained only as implementation history.

The solver integrates once per fixed substep, accumulates edge/volume/root-follow multipliers across iterations at the same step duration, and publishes an owned UUID-identified snapshot only after the full candidate passes validation. Dynamic, world-fixed and root-kinematic points are supported. Sphere contacts apply only to vertices referenced by the surface faces; internal volume controls still participate in integration, constraints and velocity reconstruction. Dynamic rigid reaction, soft-soft coupling and gas pressure are not implemented by this route and are not acceptance claims.

Final desktop verification reported by the coordinating task:

- `SoftBody.SolverTest`: 8 passed. Coverage includes analytic follow compliance and mass, multiplier accumulation, gravity, volume resistance, optional pins, failure preservation, fixed-step sequence consistency and the internal-control collision regression. The latter places a dynamic obstacle around the interior center of a five-point tetrahedral fixture: simulation succeeds with center following intact, while moving that obstacle to a surface point remains explicitly unsupported and preserves the complete previous snapshot.
- `JoltPhysicsTest`: 81 passed. These are results for the final collision-provider route and restored SDK, not the removed native softbody experiment.
- `SoftBody.WorldTest`: 11 passed, covering the CPU solver/component lifecycle and rigid-world integration.
- `WorldAssetTest`: 13 passed, including metadata-only component/subsystem persistence. Offline cook, encode/decode and validation can use persistent schemas without creating runtime components or subsystems; enabled runtime instantiation still fails when its runtime factory is unavailable.
- `SoftBody.BuilderTest`: 2 passed; `SoftBody.EditorTest`: 2 passed; module integration tests: 2 passed.
- Final registry/document rerun: `EditorCoreTest --gtest_filter=*AssetGenerator*` passed 9 tests; `EditorShellTest --gtest_filter=MeshDocumentTest.*` passed 5 tests. The earlier `*MeshGeneratorDocument*` filter matched zero tests and is not counted as a pass.
- Actual affected implementation/module builds passed. Earlier Authoring, Assets, EditorCore, Aurora cook and Mesh document checks remain recorded above; no later failure is hidden by the historical native checkpoint.
- Python CMake source ownership: 26 passed. `openspec validate add-jolt-softbody-generators --strict`: passed at this checkpoint, before this evidence-only update.

### Mobile compilation and outstanding acceptance

The installed Android NDK and OHOS Native SDK each compiled the actual independent solver translation unit and softbody descriptor validation translation unit into ARM64 object files. This verifies those source files against the selected mobile toolchains. It is not a complete application build, link, package, deployment, runtime test or performance measurement. No iOS compilation or device execution has been completed.

The agreed mobile budget is 60 FPS with ten simultaneous soft bodies. Control count remains configurable, with a 30-point seal default. Representative-device validation, full application builds and deployment on Android/iOS/OHOS, sustained timing/thermal measurements and visual quality checks are outstanding; desktop tests and ARM64 object files do not establish this budget.

Actual editor interaction and visual acceptance remain in progress in the coordinating task. Task 6.3 remains open for the seal scene against supported obstacles. Task 5.4 remains open for visual deformation-normal/shading checks even though analytic checks exist. No commit or OpenSpec archive was performed.

Continuous moving-obstacle queries require a kinematic body and `SetKinematicTarget`. Directly changing a static body's transform is a teleport, so dragging a static obstacle across the soft body is not continuous-collision acceptance. The current query implementation scans rigid bodies for each surface point and constraint iteration; its mobile performance has not been measured.

### Actual editor generator workflow and integration fixes

- Re-cooked the actual seal project with `AssetTool --cook-all --platform windows`: 13 products, zero failures.
- Fixed demo static-body authoring mass and unsupported material cull state. Actual source-world loading and Play/Stop succeeded; these checks alone did not establish seal rendering.
- The first runtime attempt found an asset UV1/0 versus shader TEXCOORD0 mismatch. The Mesh-to-render bridge now normalizes all four UV channels and rejects ambiguous additional indices without changing asset serialization. The actual Slang reflection probe showed why changing the shader semantic to UV1 would not solve it. `Aurora.AdaptorTest`: all 92 passed, including five mesh-conversion tests and existing skeletal/material tests.
- Opening the detailed seal Mesh document initially exceeded the UI snapshot vertex budget. Mesh wire/weight previews now sample within a bounded drawing budget while preserving complete source and binding data. `MeshDocumentTest`: all six passed, including a 49,152-triangle, three-primitive actual document Paint test for wire and weights with space reserved for chrome.
- Rebuilt actual `Aurora.Render`, `Aurora.Render.Editor`, `SandboxModule`, `SandboxEditor` and `EditorShellTest` successfully. A first build command also named `Launcher`, which is only a game-template target and is not configured in this engine checkout; the corrected existing-target build passed. No Launcher execution is claimed.
- In the real editor, opened the generated Mesh product, reopened its saved recipe, ran the registered generator, inspected cage/bone and weight views, saved through the toolbar, and closed/reopened the document. The reflected control budget remained 30. The actual atlas-source preview reported 30 controls, 56 tetrahedra, 74.491979% volume excess and a 0.159956 m cage-to-mesh distance upper bound. These are coarse-collider diagnostics, not a claim of close surface fit. The percentage corrects an earlier visual transcription of 74.191979%; the cooked product and exact C++ cage dump establish 74.491979%.
- The UI save wrote `seal.softbodyrig` at 02:10:20 local time; the cooked SoftBody product was rewritten at 02:10:22. Source and generated product identities remained stable. Task 6.2 is complete.
- After the UV and preview fixes, the actual Play scene displayed the complete atlas-textured seal, face accessories, skeletal rendering and shadow beside the static obstacle. Repeated Play/Stop and normal application shutdown succeeded with exit zero. `softbody-editor-acceptance.log` contains no new renderer ERROR or Jolt solver error. The edit world reports a pending/stale animation pose before Play and after Stop; the Play instance resolves it and displays the seal. This is baseline rendering evidence, not manual acceptance of large deformation or moving kinematic obstacles; tasks 5.4 and 6.3 remain open for those checks.
- CMake source-ownership checks: 26 passed; strict OpenSpec validation and `git diff --check` passed. One attempted repeat used the sandbox's inaccessible default temporary directory and ran zero tests; the repeat with the workspace temporary directory passed and is the counted result.
- The generic reflected number formatter now consumes the existing `scientific` hint for `minTetQuality`, edge compliance and volume compliance, so nonzero values such as 1e-9 are not displayed or initially edited as 0.000. The two added tests exercise formatting, parsing and actual PropertyModel edits with exact float round trips, and preserve ordinary/ranged formatting. Final `EditorCoreTest`: all 198 passed. Final `EditorShellTest`: all 154 passed, including all six Mesh document cases. Actual affected editor/SoftBody modules were rebuilt; the solver and render-input logic did not change in this formatting-only step.
- The final Debug shader deployment passed byte/identity verification and the actual seal project was cooked again with 13 products and zero failures. Android/iOS/OHOS application and real-device acceptance remain unverified.

### Known restore boundary

Independent CPU softbody snapshots do not participate in `IPhysicsWorld::RestoreState`. Rigid-world restoration can move its step counter backwards while the CPU solver retains a monotonically increasing step token. No production caller currently exercises that restore path. A caller that restores the rigid world must rebuild the CPU softbody instances/start a new session; the current implementation must not be described as including softbodies in rigid-world rollback.

## Low-control cage optimization: production verification

The actual atlas body remains unchanged: 10,336 source vertices, 20,480 source triangles, the same verified center and two exact Dynamic controls. The configurable maximum remains 30 and every body vertex retains at most four influences. The new support-plane candidate reuses the existing Authoring target; no runtime solver, Jolt SDK patch, module or target was added for this optimization.

| Production measurement | Original radial cage | Optimized support cage |
| --- | ---: | ---: |
| Controls / tetrahedra | 30 / 56 | 29 / 54 |
| Volume excess | 74.491979% | 41.2656% |
| Sampled cage-to-source distance | 0.133482 m | 0.102726 m |
| Certified cage-to-source distance upper bound | 0.135654 m | 0.104933 m |
| Rest -Y support offset, excluding radius | 0.116517 m | 1.099e-7 m |
| Minimum tetrahedron quality | 0.0217469 | 0.000388345 |
| Maximum four-weight rest reconstruction error | 6.799e-8 m | 5.968e-8 m |

The original cage was remeasured with the tighter distance certificate; its cage dump is byte-identical to the pre-measurement version. The earlier 0.159956 m value was a looser threshold-driven bound, not an observed maximum. The sampled and certified columns above use the same production measurement method. Convex filling around fin/body recesses still produces significant local surface gaps; axis support alignment does not establish a tight fit over the entire belly. The smaller minimum tetrahedron quality is reported explicitly and remains above the unchanged configured 1e-5 threshold.

- `SoftBody.AuthoringTest`: all 8 passed with the actual atlas fixture enabled. The seal uses stricter 0.50 excess-volume / 0.12 m distance limits. The new anisotropic regression uses 0.36 / 0.09 m, checks complete four-weight embedding and independently checks all six support offsets. Configurable budgets, exact mandatory positions/modes, determinism, invalid input, cancellation and failure preservation remain covered.
- `SoftBody.WorldTest`: all 12 passed. The actual generated seal ran 240 real Jolt fixed steps with the unchanged four-iteration material: slow lowering, settling, 1 cm root compression and recovery. Every step checks the actual four-weight visible vertices against the pose contribution, finite normal matrices and no more than 1 mm penetration. At steps 150, 180 and 240, visible minimum Y was 0.00500011 m, consistent with the unchanged 5 mm sphere radius. The compression peak requires nonzero contact displacement relative to root-rest presentation, so a hard root-follow implementation without contact cannot satisfy it.
- `SoftBody.BuilderTest`: both passed using the actual atlas GLB, including production import/generate/cook/reopen, stable identities and failed-build preservation. `SoftBody.AssetsTest`: all 8 passed. `SoftBody.EditorTest`: both passed.
- Actual Authoring, Builder, Assets, Editor and World implementations, `Animation.GraphAssets`, `AssetTool`, `SandboxModule` and `SandboxEditor` rebuilt successfully. The Animation asset implementation sources are now explicitly listed under their single existing static-library owner, fixing the stale generated-project omission behind the reported unresolved registration symbol. The user's separate Develop build is not counted as a build performed by this task.
- Python CMake source ownership: all 26 passed. A first attempt used the sandbox's inaccessible default temporary directory and ran zero meaningful tests; the counted repeat uses the workspace temporary directory. Strict OpenSpec validation and `git diff --check` passed at this checkpoint.
- The demo recipe now uses the stricter 0.50 / 0.12 m acceptance limits. Actor root Y is `vertexRadius - sourceMinimumY = -0.012985621467 m`; the old +0.15 m placement actively held the animal above the ground. Material mass, radius, compliance and iteration count are unchanged. Reproduction scripts preserve the adjusted placement and limits, while historical editor measurements remain explicitly historical.
- The actual demonstration was cooked again: 13 products, zero failures. Demo shader deployment passed identity and byte checks. The updated Play scene displayed the atlas seal, face details and shadow at its lowered ground placement. The actual GUI does not establish a measured 5 mm gap; that measurement comes from the automated contact regression. Interactive Generate/Save/reopen was not repeated for this optimized profile, and the editor window was left available during user interaction. Large-deformation and supported moving-obstacle visual acceptance remain open. Android/iOS/OHOS complete applications and the 60 FPS / ten-animal device budget remain unverified.

Evidence: `cmake-build-debug/softbody-proxy-*.log` and the `proxy-optimization` sibling of the seal demo artifact. No commit or OpenSpec archive was performed.

## Android deployment and benchmark preparation (2026-11-02)

The original Android installation was rejected by the phone's USB-install policy. After the user explicitly enabled USB installation, `adb install -r` succeeded on device `c07e2926` (model 23049RAD8C, Android 15, ARM64). The initial launched app then exposed a real shader-selection failure: the custom seal material only enumerated SKINNED, while the scene selected SHADOWS/IBL combinations. Normal shader cooking now covers all eight combinations: 76 total stage requirements, all source-free products verified. A baseline Debug APK using the verified original native libraries and those cooked products was installed successfully. The actual device screenshot shows the atlas-textured seal, face accessories, ground and shadow; app-scoped logs show renderer/World readiness and presentation. This establishes baseline device rendering, not Release timing or ten-body acceptance.

The Android native compiler also exposed missing libc++ support for `atomic<shared_ptr>` in the offline mesh compiler registry. The replacement uses a per-registry mutex with owned shared-pointer acquisition; unregister compares both address and ownership identity and destroys the retired provider outside the lock. All four compiler tests pass, including alias/owner and in-flight lifetime cases. This lock is unrelated to XPBD stepping.

All five exact-budget rigs were generated from the original body fixture (SHA-256 `3109301435aef71456c553c828bfe6c22d07419005134d91cb7b080e7fa4897b`). The matrix preserves the center, two mandatory Dynamic points, source mesh and maximum four weights. These performance recipes deliberately admit coarser proxies; production quality defaults and the stricter normal demonstration recipe are unchanged.

| Controls | Surface controls | Tets | Boundary triangles | Volume excess | Certified cage distance (m) | Minimum tet quality |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 30 | 27 | 56 | 50 | 41.2656% | 0.104933 | 0.000388345 |
| 25 | 22 | 46 | 40 | 51.3583% | 0.130187 | 0.00743197 |
| 20 | 17 | 36 | 30 | 78.9573% | 0.198455 | 0.00177758 |
| 15 | 12 | 26 | 20 | 105.489% | 0.229223 | 0.0013596 |
| 10 | 7 | 18 | 10 | 173.019% | 0.280163 | 0.0113809 |

Maximum four-weight rest reconstruction errors remain below 8e-8 m. The 10-point cage uses the low-budget shell and differs substantially in shape/support from the larger cages. Total mass is 3 kg for every rig, follow compliance scales with point count, radius is 5 mm and internal iteration count is four. Geometry and discretized stiffness are not identical across rows. The matrix source and exact cage dumps are retained under `build/seal-mobile/rig-quality`; fifteen Worlds (each rig with 1/2/10 soft bodies) cooked through the existing AssetTool with 33 products and zero failures, then passed source-free dependency validation.

The CPU solver now batches bodies transactionally with bilateral soft vertex/triangle contact. All 14 solver/surface tests passed. All 18 World tests passed with the actual seal fixture, including bilateral seal contact, atomic publication/failure preservation, World ownership, bounded metric history and disabled-profiling zero clock fields. World order uses stable actor identity rather than random solver instance IDs.

Additional actual-source preflight tested each exact rig for 210 fixed steps at 1/60 s, with 0.2 m/s per-body Z approach. All five air-only pairs passed (147 contact steps each). A near-ground start with only 4 mm clearance failed at frame 2 for the 15/20-point cages, before approach began; these are explicit inversion failures and are not performance results. With a 3 cm initial lift, all five pairs passed with rigid querying enabled as well. That lifted test primarily verifies initialization and pair contact; subsequent downward ground-contact workloads still require device validation. The benchmark initializes the lifted actor transforms before solver attachment, and compares each contact workload to the same motion profile with collision disabled.

Evidence: `build/seal-mobile/device-shader-fixed.png`, `device-shader-fixed-logcat.log`, `mobile-shader-variant-fix.json`, `rig-quality/matrix.json`, `impact-*-air-020.log`, `impact-*-ground-lift030.log`, and `cmake-build-debug/softbody-batch-*.log`. Release device collection is pending at this checkpoint. iOS/OHOS application/device execution and sustained 60 FPS with ten animals remain unverified. No commit or archive was performed.

### Actual optimized Release readiness

The Android Release build uses CMake `Release`, `-O3 -DNDEBUG`, Release Jolt and disabled assertions/GPU validation. Lint remains enabled. The signed, non-debuggable APK passed CRC, twelve AArch64 ELF dependency closures, 16 KiB ELF/ZIP alignment, the fifteen-World/five-rig product closure and all 76 shader requirements. Packaged assets match the staged tree byte for byte. The final test APK at this checkpoint is 48,506,203 bytes, SHA-256 `6bd6557928964e126882ed95b00e45c73299a429416054433a821e30f301e5ae`; install/update succeeded on the user's authorized phone. Device properties identify Redmi Note 12 Turbo / SM7475, Android 15/API 35; the native renderer reports Adreno 725.

Two real-device instrumentation/startup failures were corrected rather than counted as physics timings:

- The original 6000-record shared capacity filled with zero-physics-step application loops. Schema 2 now records every fixed step independently, keeps raw physics-bearing loops and exports exact totals plus bounded histograms for all loops. The original six failed runs and APK remain retained. No artificial sleep or frame pacing was introduced.
- One subsequent launch failed because normal asynchronous animation Pending state was converted to a stale-Skeleton error. Animation validates the instance identity before Ready-only Skeleton comparison; the scene bridge waits or retains the previous complete tuple during Pending. Ready mismatches and invalid identities remain explicit errors. All 95 Aurora adaptor tests and all 15 Animation World tests pass, including four new readiness/identity/retention regressions; actual Aurora.Render, Animation.World, SandboxModule and SandboxEditor builds pass.

Before the Pending fix, five of six schema-2 smoke cases completed 240 measured steps with validated raw counts and histogram sums; the failed static baseline correctly produced no paired delta. All three requested collision categories had active contacts. After the fix, a new on-phone baseline/static pair completed all 120 requested steps each. Corrected SurfaceFlinger selection found actual BLAST-layer presentation timestamps; this short evidence is not ten-body sustained-frame-rate acceptance.

The formal initial matrix uses one repetition, 120 warmup and 600 measured fixed steps per case, with 30 peak/baseline cases before 50 ten-body cases. All use 1/60 s, four internal iterations and 0.2 m/s configured approach. The currently collecting dataset is `build/seal-mobile/bench-results/full-r1`; its release evidence, plan, journal, per-run configs, app-only logs, device before/after state, step CSV, loop CSV, histograms and JSON summaries are retained. Failed, incomplete and no-contact cases must remain separate. Timings include instrumentation overhead and describe the CPU solver elapsed path; snapshot publication, pose/normal generation, animation, skin upload/GPU rendering and the native rigid step are outside `totalNs` (the native step has its own field).

The rigid contact implementation queries surface-control spheres with relative translating sweeps and endpoint overlap, not complete face/edge cage CCD. Soft-soft uses discrete bilateral vertex/triangle projection and closed-tet containment, with final crossing validation; it does not support CCD, edge-edge constraints or self collision. These low-speed pair/ground/translating-slab measurements cannot establish arbitrary-obstacle robustness. Nonzero gas pressure and dynamic rigid reaction remain unsupported.

### Ten-body render capacity gate

The first formal collection completed all thirty peak cases (26 DONE and four explicit 15/20-point rigid-impact failures), then encountered a renderer capacity failure in ten-body scenes: `sets=256`, before a native allocation failure. It was stopped and retained under `full-r1`, including interruption metadata. The thirty peak runs were copied with per-file SHA-256 provenance to `peak-r1`; no failed renderer runs were included in those measurements.

Inspection confirmed a per-lease capacity limit, not evidence of a descriptor leak. Ten animals retain all 25 primitives and their shadows, requiring roughly 500 distinct skinning draw bindings. Renderer initialization now exposes the three existing descriptor limits with unchanged defaults (1024 resources, 128 samplers, 256 sets); API and launcher input reject invalid/duplicate/out-of-range values explicitly. The benchmark requests 4096/256/2048. No model parts or shadow passes were removed, no global RHI defaults were increased, and no new target or module was added.

- Actual capacity/parameter regressions passed: six Aurora module lifecycle tests, two Renderer module validation tests, and five real GPU submission tests with one explicitly skipped unsupported default-limit case. The configured 300-binding-group test renders and validates output across five frames and frame-slot reuse.
- `SoftBody.WorldTest` now passes all twenty actual-fixture tests, including first-registration rollback and preservation of an existing coordinator after a duplicate member is rejected. The initial compile found an incorrectly inferred raw World pointer in the new test; explicit owned `WorldPtr` corrected both compilation and cleanup. Actual Aurora.Render, SoftBody.World, SandboxModule and SandboxEditor builds passed.
- The capacity APK is 48,506,251 bytes, SHA-256 `18f09078129527ca687a9f18facf99e3d41845d66eba268edf3f625dd9a9a8c3`; Release/lint, signing, ELF/16 KiB alignment and complete resource checks passed. Staged assets remain byte-identical. It installed successfully and completed all six ten-body smoke cases, including 300-particle readiness, internal-only, soft-soft and mixed soft/rigid contact. Mixed observed 61 soft-contact, 74 rigid-contact and 38 simultaneous-contact steps out of 120. These short windows are not sustained acceptance.

The uniform final matrix is collecting under `bench-results/final-r1` using that one APK, 120 warmup and 600 measured steps at 0.2 m/s. A separately labelled 0.02 m/s protocol for the 15/20-point rigid-impact failures is planned; its results must not be merged into the main speed protocol or replace its failures. No commit or archive has been performed.

### Sampler capacity and final cohort provenance

The 18f collection completed its thirty peak cases and then exposed a second ten-body setup limit in the kinematic profile: 256 sampler descriptors were exhausted (`resources=1767`, `sets=756`). These attempts had no measured physics steps and are retained as renderer setup failures. The run was stopped after 43 completed cases. All thirty peak runs were copied with per-file hashes to `peak-final-r1`; four physics failures remain N/A in its primary performance table.

Shared atlas materials currently create separate native sampler objects per draw, so sampler binding groups are not universally shared. The 1024-entry parameter/uniform caches stop accepting new entries when full and continue execution; they are not hard frame limits. For the single-view/single-shadow fixture, a conservative bound is 4034 resource descriptors, 1009 samplers and 1513 sets. The final private launcher therefore selects 8192/2048/4096. Engine defaults remain unchanged.

Only private launcher arguments changed. The new APK SHA-256 is `559c82f459b33f7f7e2c1bf7f3b4fb6c6695fe1da656d95cfeb5a309289686e7`. Independent payload comparison verifies that all eleven non-launcher native libraries (including solver, Jolt, animation and renderer), all 137 assets and the benchmark harness source are byte-identical to the 18f peak package. Both APKs and their validation records are retained in `deployment/`; `release-capacity-cohorts.json` records this equivalence and the budget difference.

The enlarged-budget package installed successfully and passed the formerly failing ten-body kinematic baseline/contact smoke, each with 120 measured steps. The definitive fifty-case crowd collection is under `bench-results/crowd-final-r1`. Final reporting combines that cohort with the thirty cases in `peak-final-r1`, records both APKs explicitly, and excludes earlier renderer setup failures. Per-profile baseline/contact pairs use the same APK, motion parameters and capacities. The separate low-impact protocol follows this collection.

## Final Android benchmark result

The accepted primary dataset contains 80 unique cases: 70 DONE and 10 physical contact failures, with zero parser/consistency errors. It includes 39,000 successful measured fixed steps in completed runs; the five physics-off baselines use wall-time windows. The twenty-case 0.02 m/s supplement contains ten completed internal baselines (6000 measured steps) and ten contact failures. Its data is analysed separately. Every failed primary timing, delta and loop statistic is N/A; truncated steps and failed-attempt timings remain diagnostic JSON/raw CSV only.

The tables below show contacting-step P99 / observed maximum in milliseconds, for the entire CPU solver batch. They include profiling and scheduling effects. They exclude coordinator snapshot publication, pose/normal generation, animation, GPU skinning/rendering and native rigid-world stepping. These are not thread CPU times or total rendered-frame times.

| Controls per animal | One soft vs static | One soft vs kinematic | Two soft bodies |
| ---: | ---: | ---: | ---: |
| 30 | 0.315 / 0.333 | 0.291 / 0.373 | 1.305 / 1.323 |
| 25 | 0.301 / 0.304 | 0.320 / 0.335 | 0.931 / 0.995 |
| 20 | FAILED | FAILED | 0.618 / 0.624 |
| 15 | FAILED | FAILED | 0.488 / 0.521 |
| 10 | 0.148 / 0.170 | 0.124 / 0.218 | 0.172 / 0.174 |

| Controls per animal, ten animals | Static rigid | Kinematic obstacle | Soft-soft | Mixed |
| ---: | ---: | ---: | ---: | ---: |
| 30 | 2.264 / 2.331 | 2.407 / 2.445 | 5.807 / 5.837 | 7.582 / 7.649 |
| 25 | 1.919 / 1.939 | 2.066 / 2.103 | 3.988 / 4.248 | 5.459 / 5.519 |
| 20 | FAILED | FAILED | 2.322 / 3.660 | FAILED |
| 15 | FAILED | FAILED | 1.426 / 1.489 | FAILED |
| 10 | 0.549 / 0.572 | 0.654 / 0.671 | 0.544 / 0.559 | 0.914 / 0.927 |

All ten primary failures concern the 15/20-point cages: nine are detected in prediction after first contact; the 15-point ten-body mixed case instead fails final inter-body penetration validation. At 0.02 m/s all ten contact cases still fail, at measured sample 82–96, during the first internal/rigid-contact pass before final export. Current instrumentation identifies the detection boundary, not which individual constraint first caused inversion. Lower speed delays/changes detection; it does not establish a fix. Global minimum tet quality is not an adequate classifier: the successful 30-point rig has a lower minimum value than the failed 15/20-point rigs.

For the 30-point ten-body soft-soft contact steps, narrowphase accounts for 88.28% of cumulative solver elapsed time. In mixed contact steps, narrowphase is 61.87% and rigid querying 19.20%. Candidate performance work is boundary-edge deduplication and exact spatial rejection; candidate stability work is per-stage failing-tet diagnostics, positive-volume protection coupled with contact feasibility, and locally validated cage improvement. Time subdivision must use consistent obstacle subintervals. Removing checks, accepting residual penetration or reducing iterations does not demonstrate stability.

The 25-point mixed P99 is about 28% below the 30-point value in this experiment, with a looser rest proxy (13.02 cm certified cage-to-source bound versus 10.49 cm). The 10-point rig is faster but has 173% rest cage volume excess and a 28.02 cm distance bound; it is not an equal-fidelity alternative. All controls are Dynamic; kinematic labels identify obstacles. Mass/follow normalization does not make discretized stiffness identical across cages.

Device evidence: Redmi Note 12 Turbo / SM7475, Android 15/API 35, ARM64, Adreno 725. Peak-stage battery temperature was 33.5–35.9 C and crowd-stage 35.5–37.9 C, with USB power and 99–100% battery. Every before/after system thermal status was zero. CPU/GPU temperature telemetry was unavailable, and frequency/temperature were not controlled. These are single-device, single-repetition initial results, not sustained 60 FPS or iOS/OHOS acceptance.

Accepted raw data and per-file provenance: `build/seal-mobile/bench-results/accepted-r1`; separate low-impact data: `bench-results/lowimpact-r1`. Reports, precise tables and figures: `build/seal-mobile/bench-analysis/{accepted-r1,lowimpact-r1}`. The demo artifact publishes `reference/benchmark/android-20261102/index.html` plus `SealSoftBody-Android-Release-Benchmark-20261102.zip` (1250 entries, 24,227,300 bytes, CRC verified; SHA-256 `0afb6e3a5edad1f0f41880d60aa3e04903fa049cd0b015cd5d476f1cdeba033b`).

The installed APK's on-device SHA-256 matches the delivered 559c package. Automatic benchmarking is disabled; a fresh ordinary launch displays the seal/ground/obstacle and reports renderer readiness and first presentation without a renderer/Jolt failure. Final Windows source ownership checks pass all 26 cases; actual modules/tests and the desktop demo's 33-product cook pass. The user's existing asset database hash remains `c8495462b4c5cea14e7df377f90210da3892487d`. No commit or archive was performed.

## Original rabbit replacement (2026-11-02)

The user reconfirmed that four bones means at most four influences per displayed vertex. The saved CuteRabbitV10 SOURCE_MODEL LOD0 was read using UE 5.8.4 and converted without remeshing or changing the original assets. Lengths and write times of the selected original uassets remain unchanged. The later BoneV1 mesh has identical rest positions; aggregating its animation-child weights into the original cage parents differs only by quantization. This demonstration uses the original 15-control binding rather than truncating that later eight-influence animation rig.

The converted source contains the complete body (5570 source vertices / 5643 corner-split render vertices / 11136 triangles) and four instances of the saved eye geometry. The full source has five primitives and 12000 triangles, with material indices `[0,1,1,2,2]`. Independent GLB decoding verifies original per-corner normals, UVs, linear RGBA and bitangents; all converted triangle winding agrees with the exported normals. Body, eyes and pupils retain distinct material slots.

The supplied cage retains 15 Dynamic points, center index 14, 24 positive tetrahedra, 50 unique edges and 16 bones including root. The original mapping computes weights at `q = 0.7 * P`, while rendering keeps `P` and applies `P' = P + sum(w * controlDisplacement)`. This does not scale the displayed animal. Eye and pupil primitives share a constant binding anchor per eye. The explicit StandardSkinning policy publishes no custom normal palette and therefore reproduces the translation-only bone normal behavior; existing AutoFit recipes keep TetGradient.

The production importer/generator/compiler fixture passes. Against all 5643 body render vertices, its maximum per-bone weight error is `2.14577e-5` relative to saved UE uint16 weights. An independent 80-case displacement audit measures maximum displayed-position error `3.79318e-6 m`; eye and pupil relative displacement remains zero. Rest binding-position error in the real generator is `5.66133e-8 m`.

This is the original coarse collision cage, not a fitted surface proxy. Its display volume is approximately `0.0955928773 m^3`, cage volume `0.3247595206 m^3`, relative excess `2.39731923`, and minimum tet quality `0.39678`. Cage-to-display sampled distance is `0.271754809 m`, with certified upper bound `0.274057997 m`. The rabbit recipe explicitly accepts finite limits `2.4` and `0.28 m`; global AutoFit defaults remain unchanged. Coverage is certified in binding space, not claimed for the unscaled display surface.

Original SI settings are mass `1 kg` per point, edge compliance `0.002`, volume compliance `2e-16`, follow compliance `0.02`, damping rate `2 s^-1`, radius `0.02 m`, friction `0.25`, restitution/pressure zero and four iterations. Volume compliance converts from `2e-8 cm^4 s^2/kg` using `0.01^4`. Recipe damping validation was corrected from an inappropriate unit interval to finite nonnegative rates, matching the existing solver contract.

The actual-parameter component/coordinator/CPU-solver/Jolt-ground regression passes 360 steps at 60 Hz: 180 settling steps and 180 steps of `0.02 m/s` root motion. It records 354 contacting steps and minimum tet volume ratio `0.990235`, while checking complete committed snapshots, positive tets, ground separation, 16 pose matrices, empty custom normals and eye-anchor skinning agreement. The fixture uses gravity `-9.81`; the demonstration explicitly uses `-9.8`. This regression uses existing test animation services and does not validate GPU rendering.

Relevant Windows results: Authoring 12 passed / one optional seal fixture skipped; Assets 12 passed; World 19 passed / two optional seal fixtures skipped, plus the separate actual-rabbit regression passed; Builder three baseline/actual-seal cases and the actual-rabbit fixture passed; Editor two passed; mesh authoring compiler six passed. Actual Assets/World/Builder/Editor/Aurora.Cook/AssetTool/SandboxModule/SandboxEditor targets link successfully. Source ownership passes all 26 cases. A temporary-directory cleanup test initially failed on a trailing-separator path comparison; checking filesystem equivalence preserves the cleanup boundary and the complete six-test compiler suite now passes.

Windows and Android each cook 14 source products with zero failures. Android shader cooking and source-free dependency closure pass. Rabbit shaders are deployed only to generated Windows output under a separately owned, reversible receipt; engine shader sources remain unchanged. The existing seal shader owner was restored alongside the rabbit owner, and the seal's 33-product Windows recook also passes.

The original gameplay controller is not ported: center-point follow modulation, extra relative-velocity damping, mode-specific step rates and gameplay contact impulses remain different. Original default clock substeps are 60 Hz; only particular gameplay modes use 120/240 Hz. Sky maxSubSteps is catch-up capacity, not an additional four-way subdivision. The earlier seal timing tables are not rabbit measurements. Android rendering/deployment verification is recorded separately when complete; no sustained mobile or other-platform acceptance is inferred from these tests.

The independent Android Release APK builds successfully in 3m17s, including lint. The 43,309,197-byte APK has SHA-256 `dab912ce0f7d68a4dead11c780f4261fe5819ace649621ba0eed3d4274924956`, application ID `com.skyengine.rabbitsoftbody`, and activity `com.skyengine.rabbitxpbd.SkyEngineActivity`. Its 12 ARM64 libraries, resolved ELF dependencies, Release optimization, non-debuggable manifest, test signature, 16 KiB ZIP/ELF alignment, 11-product source-free closure, 76 shader requirements including eight rabbit variants, archive CRC and exact asset/staging equivalence pass. Stable delivery and evidence are under the rabbit artifact's `deployment/android-release/`.

Desktop Play visually displays the complete rabbit, vertex colors, eyes, ground and shadows; the record is `reference/validation/desktop-play.png`. The unticked edit World reports an absent animation instance before Play. Slow UI updates initially appeared hung, but two native thread snapshots show advancing main/renderer stacks with Debug STL iterator-lock contention, not evidence of a fixed deadlock. No broad renderer/animation workaround was made. These observations are not Release performance data.

The first phone install attempt was rejected with `INSTALL_FAILED_USER_RESTRICTED: Install canceled by user`. After the user allowed USB installation, the retry succeeds and a cold activity start returns OK. Installed APK SHA-256 matches the delivered dab912c package. The real phone displays the complete rabbit, vertex colors, eyes, shadows, ground and obstacle; PID 9770 remains alive, with renderer readiness, the six-actor default World and first presentation recorded. No engine failure is found in the scoped process logs. Platform Gralloc capability probes, vendor/JDWP diagnostics and initial compositor dequeue timeouts remain in raw logs; successful rendering does not imply every system E/W line is absent. Evidence is under `reference/validation/android-base/`, including `installed-device-verification.json` and `device-verified.png`. The separate seal package remains installed. This is a single-rabbit startup/visual smoke, not a sustained FPS or collision-matrix result.

## Requested rabbit collision matrix

The user now requests rabbit-rabbit and rabbit-wall measurements using the earlier control-count and body-count comparison groups. This is a new dataset. The initial protocol retains 30/25/20/15/10 controls, at most four influences, a single-pair/single-body stage before ten bodies, matched-motion internal/off baselines, 120 warmup and 600 measured fixed steps, 60 Hz, four iterations and 0.2 m/s approach speed. Failed or noncontact cases keep explicit failure/N/A outcomes. Static and kinematic obstacle tests use a vertical wall and horizontal approach; the earlier seal ground-compression workloads are not the same scene.

The benchmark artifact is separate from the verified base rabbit demonstration. The 15-point cage is retained exactly; higher-budget candidates refine boundary sampling on the same physical convex hull, while the ten-point candidate may change proxy shape and must report that difference. Every candidate requires actual production generation/compiler validation and measured geometry diagnostics. Total mass stays 15 kg using per-point mass `15/N`; per-point follow compliance `0.02*N/15` holds aggregate root-follow stiffness constant. Edge/volume discretization and collision sampling still change with topology, so these normalizations do not establish identical material response or collision fidelity.

### Production rigs and frozen collection protocol

The optional matrix fixture in the existing `SoftBody.BuilderTest` passes the real importer, generator, physics configuration and Mesh/Skin/Skeleton compiler for all five recipes. In descending control-count order, actual tetrahedron counts are 54/44/34/24/14 and unique edge counts are 110/90/70/50/30. All controls are Dynamic, with one interior center and N-1 boundary points; every vertex retains at most four influences. N15 is the unchanged reference recipe. N20/25/30 use exactly representable float midpoints and preserve the original nodes and convex hull without relaxing convex validation or padding it outward. Their cage volume is `0.32475952059030533 m^3`; the certified cage-to-source distance bound remains about 0.274 m. N10 has a different hull and about 0.247 m distance bound. More controls therefore improve boundary sampling rather than hull fit.

The independent benchmark APK is 45,474,605 bytes with SHA-256 `fa3d3b7bf263e9c219101f254b29a7c6d10cc2c3dc18d70b14f7916a6850c527`. Release/lint, twelve ARM64 libraries, 16 KiB ELF/ZIP alignment, v2 test signing, non-debuggable manifest, source-free 46-product resource closure and exact staged-asset equivalence pass. All eleven base products remain byte-identical. The installed hash matches this package before collection. Eighteen separate 120+180-step device smoke cases are DONE with zero analysis errors; they are excluded from formal timing tables. Ten-body mixed smoke records 83/180 simultaneous-contact steps at N30 and 79/180 at N10, with zero contacts in their matched internal baselines.

The accepted dataset is `build/rabbit-benchmark/bench-results/accepted-r2/{peak,crowd}`. An earlier partial pilot is retained separately and excluded: a host log observer inspecting only the last 64 KiB could miss an already emitted completion marker under Vulkan diagnostics, adding post-measurement waiting and changing thermal history. No native code, APK or physics protocol changed. The replacement `rabbit-android-collector-v2` reads incremental byte-offset UTF-8 snapshots, retains the first terminal marker and keeps the existing ADB polling intervals. Its frozen driver SHA-256 is `98570662def17ac9ba6518d9ec2f82b7568cf520be028f14f831f885634f9532`; synthetic cases and a replay of the actual 16.7 MB log pass. Both formal stages use this collector, with version/hash retained in plans and per-run evidence.

For each cold launch, the successful probe step is excluded; 120 warmup steps follow, and the first of 600 measured steps is `motion_index=1`. The motion listener runs before the softbody coordinator. Baselines match relative motion phase and per-step commanded root/wall speeds; absolute native step IDs need not match across launches. The 0.2 m/s limit applies per motion channel: pair-relative motion and mixed superposition can reach 0.4 m/s. Six contact windows are retained (soft, rigid, soft-only, rigid-only, either, simultaneous). Mixed completion requires a measured simultaneous contact, and its primary window is either contact. The ten-second measurement window is a fixed scripted window, shorter than the mixed trajectory's 60.2-second joint period.

The measured total is elapsed CPU softbody solver-transaction time, including temporary scratch allocation, validation, XPBD, collision-provider calls, projection, finalization and snapshot commit. It excludes World coordination/publication, interpolation, pose generation, animation, skinning/rendering and the independent native Jolt step. Eight nonoverlapping buckets are contained in that total and leave a bookkeeping residual. Profiling adds overhead. Native rigid timing and process-wide loop CPU/wall timing are separate; neither summing quantiles nor converting loop duration to presentation FPS is valid. Presentation timestamps are not collected in this protocol.

The original collision cage extends roughly 13.5 cm beyond the visible rabbit on each Z side, before the 2 cm control-point radius. A scientific figure decoded from the original GLB and production N15 cage illustrates this difference. Rest/nominal target geometry still has about 30.5 cm visible bounding-box separation at the pair's 15 mm closure and about 12.5 cm visible-to-wall gap at the 40 mm wall stroke. Those are geometric illustrations, not measured solved shapes. Active projection counters prove proxy contact; they do not prove visible high-resolution surfaces touch.

### Completed rabbit Release matrix

The accepted collector-v2 cohort completed all 80 unique cases: 30 single/pair cases followed by 50 ten-body cases, all DONE with zero parser/consistency errors. There are 75 physics cases with 45,000 successful measured steps and 9,000 warmup steps, plus five physics-off wall-time baselines. All 35 collision/internal pairs match motion phase and commanded speeds. An independent second implementation reproduces the six contact-window distributions, bucket quantiles, loop/histogram accounting, geometry and collector/device/package provenance without using ADB. Smoke and the superseded pilot are excluded.

Each cell below is contacting-step P99 / observed maximum in milliseconds for the complete CPU softbody solver transaction, not rendered-frame time. Mixed uses the union of soft and rigid contact steps.

| Controls per rabbit | One rabbit vs static wall | One rabbit vs kinematic wall | Two rabbits |
| ---: | ---: | ---: | ---: |
| 30 | 0.299 / 0.410 | 0.262 / 0.271 | 1.308 / 1.326 |
| 25 | 0.250 / 0.278 | 0.224 / 0.231 | 1.066 / 1.084 |
| 20 | 0.232 / 0.249 | 0.223 / 0.256 | 0.736 / 0.902 |
| 15 | 0.177 / 0.243 | 0.186 / 0.202 | 0.412 / 0.425 |
| 10 | 0.181 / 0.241 | 0.177 / 0.209 | 0.155 / 0.170 |

| Controls per rabbit, ten rabbits | Static wall | Kinematic wall | Five soft pairs | Mixed |
| ---: | ---: | ---: | ---: | ---: |
| 30 | 1.839 / 1.892 | 1.857 / 1.897 | 7.622 / 7.663 | 8.758 / 8.790 |
| 25 | 1.580 / 1.619 | 1.628 / 1.662 | 5.351 / 5.443 | 6.251 / 6.388 |
| 20 | 1.283 / 1.661 | 1.320 / 1.365 | 3.343 / 3.456 | 4.084 / 4.147 |
| 15 | 0.980 / 1.015 | 1.068 / 1.151 | 1.841 / 1.860 | 2.856 / 2.881 |
| 10 | 0.837 / 0.852 | 0.834 / 0.889 | 0.892 / 0.908 | 1.445 / 1.520 |

For ten rabbits, the same-motion stepwise increment P99 is 7.132708 ms for N30 soft-soft and 8.305833 ms for N30 mixed; the N15 equivalents are 1.572135 and 2.558906 ms. These are quantiles of per-phase differences between separate runs, not differences of quantiles or isolated collision-function costs. Raw negative differences are retained. Independent native Jolt contact-window P99 values for N30 are 0.028698 ms static, 0.108750 ms kinematic, 0.037031 ms soft-soft and 0.032083 ms mixed; these quantiles must not be added to solver quantiles.

The current soft-soft narrowphase and final crossing checks dominate: they occupy 88.84% of cumulative contacting solver time for N30 ten-body soft-soft and 76.45% for N30 mixed. The corresponding N15 values are 77.19% and 56.36%. The original N15-to-N30 pair comparison has a 3.18x observed P99 increase while retaining the same convex hull. Candidate optimization work is spatial rejection and duplicate boundary-test removal; increasing control count alone does not improve cage fit. Static/kinematic wall queries occupy roughly half of contacting solver time. Single-run static/kinematic timing differences do not establish which obstacle type is generally cheaper.

All mixed cases contain measured simultaneous soft/rigid contact; N30 has 281 simultaneous steps and N15 has 276. The largest successful measured final geometric contact residual across the matrix is `4.9311667680740356e-5 m`, below the current zero-contact-compliance 0.05 mm tolerance. This metric is sampled during final soft validation and final-float rigid queries, not a pre-projection penetration or impulse peak. Active projection counters accumulate across constraint iterations and are not unique contact counts.

The device is the same Redmi Note 12 Turbo / SM7475 / Adreno 725, Android 15/API 35, ARM64. Peak-stage battery temperatures are 37.3-37.9 C and crowd-stage temperatures 37.6-39.9 C; all before/after system thermal statuses are zero. The test uses USB power, one descending-N repetition, no randomized order or locked frequency, and no controlled CPU/GPU temperature. Battery telemetry and thermal status zero do not establish equal chip conditions or absence of throttling. Loop timings only cover the measured Application::Loop intervals; Prepare/Collect and work between those intervals are excluded. Core-equivalent ratios are not whole-test average CPU utilization. No sustained 60 FPS, iOS or OHOS acceptance follows from this dataset.

After collection, the external benchmark config is `enabled=false`. A fresh ordinary launch succeeds with PID 18383, the six-actor default rabbit World, renderer readiness and first presentation, with no benchmark auto-start. Root visually inspected the complete rabbit, eyes, vertex colors, ground, obstacle and shadows. The installed APK hash still matches the frozen fa3 package, completing the before/after cohort bookend; it is not per-case attestation. Scoped engine-failure checks pass while vendor/Gralloc/compositor diagnostics remain in raw logs.

Rabbit-only analysis and figures are under `build/rabbit-benchmark/bench-analysis/accepted-r2`. Artifact evidence is `rabbit-softbody-benchmark/reference/validation/android-formal-audit/` and `android-post-benchmark/`; the report is published separately with these attachments. CMake source ownership passes 26/26 using the workspace temporary directory. The user's asset database git blob remains `c8495462b4c5cea14e7df377f90210da3892487d`. No commit or OpenSpec archive was performed.

Final publication is `rabbit-softbody-benchmark/reference/benchmark/android-20261102/index.html`, with tables, figures, six contact windows, phase-paired increments, independent Jolt timing, the full independent audit and ordinary-demo restoration evidence. Its adjacent `android-20261102.zip` contains 1009 entries / 27,448,477 bytes, SHA-256 `5d4e812dc4d68d864175bd6bd56a03f869d38e082b04bb0d79fad9358cfad6d3`. CRC and duplicate-entry checks pass. Root independently compares all 964 accepted raw files (159,931,897 bytes), every included report file and every Python script against the ZIP; all bytes match. Recursive HTML link validation finds no missing local targets. The partial pilot journal is included only as an exclusion record; pilot/smoke raw measurements are absent from the formal archive. The report explicitly identifies mixed as soft-soft plus a static wall; only the separate rigid-kinematic profile moves its wall.


## Ordinary two-rabbit collision and IBL (2026-11-02)

The user's current Android Studio project remains at `build/rabbit-benchmark/build/android`; its default descriptor now selects `scenes/rabbit-collision.world`. The new demonstration's reproducible inputs and separate delivery are under `rabbit-collision-demo`. The seven-actor World contains two original rabbits, static ground, directional/point lights, camera and EnvironmentComponent. Each selected recipe has 15 Dynamic controls and at most four influences; the ordinary driver requires two bodies without hardcoding the control count. Original source, eye anchors and StandardSkinning remain intact.

Two inward-facing roots at Y=0.524 m start 1.1560254 m apart on X, preserve identity/Y180 rotation and close by 0.255 m total at 0.2 m/s per rabbit. After initial settling, each cycle takes 3.275 s with one-second holds at both ends. Optional external `rabbit_demo.json` configures closure, speed, holds and settling; absence of this file runs native defaults. DemoSession consumes existing Physics World control/listener interfaces in the existing external Launcher target; no engine module/target or backend implementation was added. Initialization returns PhysicsStatus and failure follows the launcher's normal finish/shutdown handshake. Ordinary mode enables soft-soft and static-ground contact without benchmark profiling/history. After-step staging is safe because presentation later derives model-space poses from the current actor root.

The original default World had no EnvironmentComponent. Its fallback ambient value was 0.03, while hdr=false on the native UNORM output omitted the existing tone-map/display-encoding path. The new World enables HDR with intensity 1 and zero EV adjustment. It consumes actual linear RGBA8 KTX assets: 64x32 diffuse E/pi, 256x128 specular with nine GGX roughness-prefiltered levels, and a 128x128 split-sum BRDF LUT. A deterministic 1024-sample project generator matches the shader's Y-up lat-long and dielectric F0=0.04 conventions. Constant-convolution, linear-Y irradiance, direction round-trip, KTX payload and repeat-generation checks pass. The procedural source is bounded linear LDR, not an HDRI or a sky-background renderer. Per-texture settings retain linear color and all authored mip levels. Fixed 0.18 material fill is removed; direct lights and IBL provide illumination.

Independent staged validation caught a real importer error before final deployment. The correct source specular tail was 1x1 RGBA8, but ImageSource::MipDim used `base > level` before shifting, so 128>>8 became zero. The resulting cooked final slice lost four bytes. The production helper now checks shift width and clamps the shifted extent to at least one. Existing CookPipelineTest regressions exercise full 256x128 KTX1 and KTX2 chains through LoadKtx, WriteImageAsset and ConvertSceneImage, checking every pixel, all slices and the terminal offset 174760/size 4 with total bulk 174764. Actual Aurora.Cook, AssetTool and Aurora.CookTest builds pass; all 59 cook tests pass. Source ownership passes 26/26. The original failed staged check is retained; no runtime threshold or mip count was relaxed.

A first APK launch also exposed a project/manifest/native-library mismatch after a display-name change. It failed before entering the engine because GameActivity requested libRabbitCollisionDemo.so while the existing Launcher emitted libRabbitXPBD.so. The original project name was restored. The delivery verifier now decodes the APK's actual MAIN/LAUNCHER activity metadata and requires android.app.lib_name to resolve to an existing packaged ELF. Its negative regression rejects the archived 24947 package. A working 321e intermediate, with three actual soft/ground-contact cycles, is archived separately from that failed package.

The final data-only polish uses a 12x12 m ground and camera at [0.45,2.8,2.85], aimed at [0,0.55,0] with 34-degree vertical FOV. This reduces the oversized ground bounds that degraded the current directional shadow projection. At the phone's 2.222 aspect ratio, all viewport corner rays intersect the ground within its bounds (minimum edge margin 0.284 m), and the actual full rabbit bounds fit throughout approach/separation. The APK's cooked World is independently decoded to verify these values.

The final Release APK is 45,522,969 bytes with SHA-256 `d7e2f970ead729c0e1cdeabb4e1c016f89f00b7b41d527b278a0b02e68a65e22`. Signature, non-debuggable manifest, 12 ARM64 libraries, 16 KiB ZIP/ELF alignment, native dependencies, exact staged assets, 50-product source-free closure, full World/environment metadata, three complete linear IBL payloads, eight Rabbit variants and ToneMap stages pass. Release O3/NDEBUG, assertions disabled and Release Jolt are recorded. Native ELF and IBL product bytes match the previously working intermediate; only World/index resources changed in the final polish.

Installation succeeds. A cold launch runs as PID 2594, reports the seven-actor default collision World, renderer readiness and first presentation, and stays alive through capture. DEMO_READY reports soft=1/rigid=1/profile=0; actual geometry reports two bodies/30 particles. Three successive cycles record actual soft and ground projections at steps 335,531,728. The installed APK hash matches delivery. Benchmarking is disabled and external rabbit_demo.json is absent, validating the native default behavior. Root inspects both 2400x1080 near/separated screenshots: two complete inward-facing rabbits, eyes/colors, environment fill, ground and sharper shadows. Scoped engine failure checks find no failure; vendor/Gralloc/JDWP diagnostics remain in raw logs. This is startup/visual/repeated-contact evidence, not sustained FPS or iOS/OHOS acceptance. The original coarse proxies still permit a visible surface gap at proxy contact.

Delivery and device records: `rabbit-collision-demo/deployment/android-release/` and `reference/validation/android-device-final/device-verification.json`; the final scene source and generators are retained beside them. The earlier fa3 benchmark APK, its 80-case data ZIP and native benchmark source remain byte-identical. No commit or OpenSpec archive was performed.

### Revised ordinary demo acceptance on Android

The previous d7e package passed proxy-contact checks while the retained near screenshot still had 152 pixels of background between the fur silhouettes. The old cage projected about 15.3 cm beyond each rabbit nose including its particle radius.

The revised 950417d9 package uses a separate nose-fitted 15-dynamic-control rig, 16 bones including root and at most four weights per vertex. The original GLB is used unchanged. A generator preserves binding-space coverage over all triangles at a binding position scale of 0.89. Only the new recipe uses a 0.01 m radius, 0.01 follow compliance and 0.008 edge compliance. The two rabbits use the same new cooked rig, mesh, skin and skeleton. The camera looks onto the collision plane from zero X offset. The formal benchmark data still describes the original benchmark rigs.

On the Redmi Note 12 Turbo the installed Release hash was 950417d93368dfc748c6eb0a19967caca06bc696c74383f08420f0c2141724e5. The opt-in capture recorded both actors' current render poses at steps 284, 365 and 463. Reconstruction from the signed APK covered 11,136 indexed body triangles per actor. Nose surface distance was 191.00–191.12 mm before approach. At sustained loading the two nose surfaces had an explicit triangle intersection, with 2.41 mm overlap along the collision axis. The left/right fixed nose-to-midbody measurements shortened by 9.69/9.42 mm. On separation the surfaces lay 183.85–183.90 mm apart, and both local shape measurements recovered to within 0.14 mm of their initial lengths.

After removing the diagnostic override, a cold launch reached the default World and first presentation and remained alive through capture. Three successive approach cycles registered solver contact. Ordinary screenshot sampling produced a near image with continuous light fur across 17 scanlines and a far image with a 126-pixel gap. The evidence includes a 6.93-second on-device clip of the ordinary mode. Diagnostic poses, checks, images and video are in rabbit-collision-demo/reference/validation/surface-contact-device/. The signed package is under rabbit-collision-demo/deployment/android-surface-contact/.

### Back-to-back collision and debug overlay on Android

The Redmi Note 12 Turbo ran Release APK 9aa07406c3ef0bf87a80a841e17f27113963b8d797503dd26c6fd30e31653a01. Each root began moving inward at 0.80 m/s. At 1.60 m initial root spacing the backs had a 1.137 m axial gap. At the 0.40 m hold the rear skin triangles intersected, and the two rear-to-midbody lengths shortened by 12.19 and 11.47 mm. After separation both measurements returned to within 2.77 mm of their initial lengths. A cold start without the verification override showed three independent collision cycles.

The Android switch enabled, cleared and restored the collision visualization in the same process. Its first enabled native frame contained 30 points, 100 edges, 48 soft surface triangles and 12 Jolt ground triangles. All 48 consecutive enabled screenshots showed 30 particles. At compression and recovery, respectively, the independently projected animation poses matched 30/30 particle centers with maximum errors of 0.53 and 0.62 pixels on the 2400 × 1080 screen. The disabled scene contained zero sampled point, spring or ground-line pixels. Some ADB touch attempts required a retry. Frame capture and collision visualization remained disabled for benchmark runs.

The signed build is under `rabbit-collision-demo/deployment/android-back-impact/`. The mesh checks, switch events, projection report and eight-second recording are under `rabbit-collision-demo/reference/validation/back-impact-device/final/`. The Win32 SoftBody.World build and tests passed with 23 tests passed and two optional seal fixtures skipped.

### Stronger recoil in the ordinary Android demo

The Redmi Note 12 Turbo ran both variants. The baseline diagnostic APK 2bf96a9b used the original 9aa07406 assets and motion with continuous pose capture added. The final Release 471ac4f250539c4d0a17db8a43d68b33da0ce821731a359e332fcc6a10fc78bd used the revised material and motion. The phone provided 229 baseline and 224 revised published poses, with no missing fixed steps within either recording. The rear skin triangles intersected in the revised trace. Both root approach speeds were 0.80 m/s.

Over the first 0.70 s of soft contact and release, the peak-to-peak horizontal ear-tip displacement relative to the middle of the body, measured on the mean of the two rabbits' curves, rose from 34.55 to 96.47 mm. The corresponding peak-to-peak rear-to-midbody length change rose from 29.86 to 44.13 mm. During the revised 0.75 s intermediate hold, both roots were stationary and there was no soft contact. The backs moved 25.61 and 25.30 mm between opposite extrema over 0.25 s. The hold resolves one reversal and does not establish a full decay rate.

The ordinary run uses no diagnostic file. The previous debug switch was exercised and left off. The comparison trace and contact-aligned ordinary-mode recordings are in `rabbit-collision-demo/reference/validation/jelly-rebound-device/`.

### Matched 15-point jump and material ablation

The original JellySolverCore implementation and Sky's production CPU solver were given the same root translation with the same 60 Hz step, a 3.5 m/s launch, 9.8 m/s² downward gravity and a flat ground. The two sets of measured control positions were used to skin the same 5,643-vertex rabbit body using each rig's weights. The UE capture in this controlled test represents the ordinary isolated XPBD rabbit. Its reference surface uses the original cage and weights without locomotion animation. The packaged UE SoftBounce scene has four substeps per 60 Hz tick, contact-driven horizontal root recoil and a special center attachment. It is a separate reference for the collision comparison.

| Surface height in centimeters | Standing | Minimum after landing | Height lost |
| --- | ---: | ---: | ---: |
| UE ordinary isolated XPBD | 86.78 | 78.47 | 9.6% |
| Sky prior rear-impact material | 83.01 | 48.43 | 41.7% |
| Sky with UE material, same Sky cage and binding | 89.22 | 78.50 | 12.0% |

Changing only the Sky edge compliance from 0.019 to 0.002 raised the minimum height to 74.73 cm. Changing only damping from 0.8/s to 2/s gave 55.76 cm. Changing only follow compliance from 0.014 to 0.020 gave 45.92 cm. The old Sky cage's total signed volume stayed between 99.959% and 100.192% of its rest value during the measured jump. The pronounced flattening therefore coexisted with almost constant volume. The common-camera replay, three-way landing frame, raw trajectories, inputs and measurement code are under `rabbit-collision-demo/reference/validation/ue-sky-jump/`.

### UE and Android grounded pair

The original executable ran the XPBD `ModeSoftBounce` pair at 120 cm/s per rabbit, compression 1 and rebound 0.5. The `wall=false` case in `reference/original-ue-xpbd/xpbd-frames/result.json` measures mean trunk shortening from the neutral skin. Both rabbits' collision-axis extrema are taken over vertices within 32 cm above or below the root. The Android replay uses the signed APK's mesh, skin and published bone matrices. The original samples at 30 Hz and the phone captures up to 60 Hz.

| 15-point pair | Maximum mean trunk shortening | Peak relative opening speed | Maximum lateral width / neutral |
| --- | ---: | ---: | ---: |
| UE XPBD | 4.864 cm | 0.946 m/s | 1.182 |
| Android 5d3d8793, timed drive | 1.693 cm | 2.000 m/s | 1.059 |
| Android aaee9c85, contact-driven candidate | 3.005 cm | 0.980 m/s | 1.123 |

On the candidate the proxy pair first contacted at step 229, the actual rear skin triangles intersected at step 230, and mean shortening peaked at step 234. The two roots were already opening while the skin continued compressing. A separate production-solver experiment changed only the center point's follow compliance multiplier on the 15-point cage. At multipliers 1, 0.01 and 0.0001 the maximum trunk shortening was 3.170, 4.249 and 5.537 cm. These isolated runs establish the center's contribution. The UE contact mode also damps center velocity relative to its root and uses moving peer planes.

The candidate failed repeated-contact acceptance on the Redmi Note 12 Turbo. In a cold start, cycle 1 released normally and cycle 2 failed final inter-body penetration validation 0.134 s after the root-support impact. No jump had launched. A separate session after two jumps failed during cycle 3. The first-cycle measurements above remain valid for this candidate. The failing log and installed hash are in `reference/validation/rabbit-impact-device/before-fix-repro/`.

The first bounded cleanup build 7435a3a8 still stopped at cycle 2 on the phone. A subsequent 60 Hz build 434dac13 completed 20/20 contacts and releases with two launches and landings in one ordinary 15-point session. The 20-point HUD switch exposed a different residual of 0.603059 mm after damping was restored, recorded at step 10568 with no point center inside the peer. A true Jolt floor box also reproduced failures that an ideal analytical floor missed. This build is retained as a diagnostic comparison. The failing step remains unpublished.

The staged 120 Hz Release is `deployment/android-rabbit-impact-120hz/RabbitXPBD-RabbitImpact-120Hz-Android-Release.apk` (SHA-256 `5dc668eee34e581b90172391311ca584245c8408fe7dc7038d37a9994d0c2ec2`). The decoded ordinary World has a 1/120 s interval and up to eight fixed steps. All five rigs retain four internal iterations. Compared with 434dac13 the twelve native libraries and DEX are byte-identical. The runtime asset changes are the ordinary World and its closure record.

The Win32 production solver and actual Jolt floor box completed 850 consecutive encounters across five point counts, in ten runs of 85, with 150 jumps in the mixed runs. Each encounter had bilateral solver contact. At the default 15 points the 120 Hz host measured 3.228 cm maximum mean trunk shortening versus 2.971 cm on the 60 Hz host under the matched test. Rear skin triangles intersected in all five rigs. At 60 Hz the 20-point phone damping-switch failure of 0.603059 mm was reproduced on the host at 0.605925 mm with the same point, peer and triangle. The corresponding 120 Hz schedules completed across the tested startup offsets. See `reference/validation/jolt-fixedstep-offline/summary.json`. The 120 Hz APK was ready for device validation when the Redmi disconnected from USB.

The additional 120 Hz host material tests covered 50 endpoint runs of 35 encounters each and 10 adjacent HUD-edit runs of 85 encounters each. Together with the default profiles this gives 3,450 confirmed encounters and 1,050 jumps. The runtime, pose and failure evidence distinguishes Win32 Jolt measurements from Android observations.

### Replacement phone, 120 Hz Release

The Xiaomi 17 Pro Max (2509FPN0BC, Android 17) installed the previously signed APK `5dc668eee34e581b90172391311ca584245c8408fe7dc7038d37a9994d0c2ec2`. Its runtime reported a 0.0083333 s fixed step. The first grounded encounter supplied 519 consecutive published poses in cycle 1, with the jump beginning after that encounter. Mean peak trunk shortening was 3.319 cm, versus 4.864 cm in the UE reference. The peak relative opening speed was 0.983 m/s, versus 0.946 m/s in UE. The two rendered rear patches intersected in 31 sampled poses. The ordinary recording, UE comparison, capture recipe, measured poses and analysis are retained in `reference/validation/rabbit-impact-device/final-poses/`. Earlier 60 Hz device comparisons were measured on the Redmi.

In one ordinary session, the 15-point pair completed 30 consecutive contacts and releases. Two button-triggered jumps landed during the run. Each encounter has a positive solver contact count and a matching release in `reference/validation/rabbit-impact-device/final-endurance/endurance.json`.

The Xiaomi main screen recorded two further ordinary jumps. The second press during flight produced an airborne rejection, and the next press after landing launched both rabbits again. A separate take shows the points, springs and collision surfaces during a jump. Both views recorded a rise of 0.6105 m and both rabbits grounded at landing. The videos, touch timestamps and native events are in `reference/validation/rabbit-impact-device/final-jump/`. The checker selected the physical main display and converted its embedded Display P3 profile to sRGB before counting overlay pixels. The debug view had 1,471 cyan and 56,330 orange pixels in the measured region, and the ordinary view had zero of each.

On the same running process, the HUD switched both rabbits through 10, 20, 25, 30 and 15 control points. At each size the test changed damping from 2 to 2.15, restored it, and jumped. After landing the 20-point pair completed ten consecutive contacts and releases. The other four sizes completed two each. The HUD also exercised seven volume compliance settings. The final pixel check found 1,524 cyan and 56,152 orange pixels with the overlay enabled and zero with it disabled. The complete trace is in `reference/validation/rabbit-impact-device/final-controls-srgb/release-check.json`. The same Release was left in the foreground with 15 points on each rabbit, four iterations, the default material and debug drawing off.

### Ordinary pair performance on the Xiaomi

The illuminated back-impact scene used the same two rabbits, original root drive, grounded rigs, 120 Hz World and four solver iterations per rabbit throughout. One condition added a visible static box 5 cm thick at the midpoint. The timing Release changed only the launcher binary relative to `5dc668ee`; all 172 packaged assets and all physics libraries matched. Two cold starts per condition used opposite test orders, 240 warmup steps and 3,600 timed steps each. The 20 runs yielded 72,000 contiguous measured steps and 156 complete impacts. The device returned to the ordinary signed APK after collection.

| Controls per rabbit | Rabbit pair contact P99 / max (ms per physics step) | Pair against divider P99 / max (ms per physics step) |
| ---: | ---: | ---: |
| 10 | 0.486 / 0.526 | 0.431 / 0.595 |
| 15 | 0.830 / 1.035 | 0.392 / 0.494 |
| 20 | 0.943 / 0.971 | 0.622 / 0.910 |
| 25 | 1.591 / 1.694 | 0.542 / 0.562 |
| 30 | 2.472 / 2.650 | 0.747 / 0.791 |

The table measures elapsed CPU time in the profiled soft body batch for both rabbits. Divider contacts require a committed particle surface within 0.1 mm of a verified box face and a rigid projection in the same step. Floor contacts alone do not qualify. Adjacent steps for the 15-point pair needed 1.423 ms at P99 during rabbit–rabbit contact and 0.774 ms against the divider. The 30-point values were 4.370 and 1.339 ms. The render and presentation paths are outside these measurements. Raw step data, per-encounter definitions, rig readbacks and device evidence are in `reference/validation/ordinary-pair-performance-20261103/`.

### Mesh document surface preview on Windows (2026-11-03)

The previous preview sampled only one out of every six triangles in the 12,000-triangle rabbit. The Debug Mesh editor now renders all indexed primitives into a depth-tested surface image and composites the control, bone and cage overlays above it. The actual `Project/rigs/demo-back` Mesh was reopened and generated in memory. The unweighted view displayed continuous body and foot surfaces, and the selected bone's heatmap showed a continuous transition across the lower body and foot. The preview reported 15 controls and 24 tetrahedra. The recipe was not saved.

The Debug EditorShell suite passed all 155 tests. Regression fixtures check interior coverage of a 49,152-triangle multi-primitive mesh in both display modes and verify that changing primitive submission order preserves the visible foreground weight gradient. UITextTest passed all 26 tests, including registry expiration, and the CMake source ownership suite passed all 26 tests.

### Shared Aurora preview stage and material responsiveness (2026-11-04)

The Windows Debug Mesh document opened the saved 15-point rabbit without regenerating or saving its recipe. The default view displayed its cooked materials, control points and bones, a ground plane under its feet, and a cast shadow. Switching to a bone weight kept the silhouette, camera, ground and shadow in place. The desktop check also exercised wheel zoom and the frame toolbar button. The Chinese IME intercepted the F key in the interactive check. Camera and shortcut tests cover viewport framing, orbit, pan, wheel zoom and unmodified F. The material window was left open.

Before the fix, a live sample of the unresponsive window thread showed `EditorRenderer::Tick → EditorPreviewSessions::Append → SceneAssetSource::MaterialPayload → CompileShaderTechnique → ShaderCompilerSlang::Compile → Slang::linkAndOptimizeIR`. It subsequently returned to the message pump. The preview now polls owned background work and shares matching programs across materials. During a fresh Debug run the weight view responded before the material appeared. Over 89.849 seconds, all 398 WM_NULL probes to the Mesh window returned within the 750 ms timeout, with P95 34.466 ms and maximum 84.247 ms. These values measure window message response, not frame time or time to first shaded frame. Cold shader preparation still takes tens of seconds on this host.

EditorShellTest passed 157 tests, EditorWidgetsTest 43, Animation.EditorTest 8, and the ViewportControls filter 12. The 5 PreviewSessions tests passed, including staging isolation, stable ground bounds and existing lighting. The complete EditorRenderTest suite passed all 47 on repeat after a time-bounded picker query missed its deadline on the first run. The 3 asynchronous material regression tests, 26 CMake source ownership tests and strict OpenSpec validation passed. The repository has no particle editor yet. Future effects can use the same preview session and supply known effect bounds.

Desktop evidence is under `rabbit-collision-demo/reference/validation/mesh-editor-preview-20261104/` in the example handoff, including the material and weight screenshots, sampled UI stack and message response series.

### Static mesh preview navigation and interactive cost

The Win32 editor opened the 15-control rabbit at `Project/rigs/demo-back/mesh` in the normal shaded view with bones visible. With the mesh already shaded and the camera stationary, 21 of 40 main-thread stack samples included the Mesh document tick. Eleven included refreshing generator actions and nine included checking file timestamps. On the renderer coordinator, 30 of 40 samples included building a frame. These are inclusive stack counts.

The measurements below use 12-second Windows DxgKrnl Present traces filtered to the SandboxEditor process and the floating Mesh window. They measure intervals between window submissions, excluding the main window and the cold shader-loading period.

| Configuration | Window presents | Mean interval | P50 | P95 |
| --- | ---: | ---: | ---: | ---: |
| Debug before | 215 | 55.90 ms | 53.64 ms | 65.88 ms |
| Debug with document, view update and UI packet changes | 252 | 47.50 ms | 47.22 ms | 50.06 ms |
| Develop with the same changes | 3783 | 3.17 ms | 3.10 ms | 4.53 ms |

Both Debug rows use the Debug runtime and GPU validation. Develop uses optimized code without the Debug GPU validation layer. The configuration comparison does not isolate the cost of either setting. Presentation submission cadence does not measure the monitor refresh rate or input-to-display latency.

A separate 18-second Develop capture included intermittent wheel zoom in and out in the same Mesh window. Across 6635 presents, the mean was 2.71 ms and P95 was 3.53 ms. The camera returned to the starting view and the control and bone overlays followed the material image throughout the zoom.

On the final Develop launch the rabbit was fully shaded at the first follow-up observation, 10.75 seconds after opening the mesh. This is an upper bound from visual sampling. The editor and preview use asynchronous material compilation, with their original asset-loading policies. The built-in UI program still compiles during editor initialization. The material program cache is scoped to the process.

Final Debug regressions passed for EditorShellTest (158), EditorRenderTest (48), EditorWidgetsTest (44), Animation.EditorTest (8), and the selected ViewportControls and AssetGenerator tests (22). The three asynchronous scene material cases and the UI rendering contract passed in Aurora.AdaptorTest. Source ownership passed all 26 tests and the OpenSpec change passed strict validation.

### Commit preparation after the animation rebase 2026-11-10

The submission candidate is based on `983c581a`. Review covered the Physics/softbody/Jolt, Animation/Framework and Aurora/Sandbox integration boundaries. Changed C++ ranges were formatted with the repository configuration; the 147 changed C++ files contain no non-ASCII characters. No new targets or directory moves were introduced during this preparation.

Two regressions were corrected. `ShaderFileSystem::SnapshotProgram` now preserves owned loose virtual files, including dependencies reached through macro includes or Slang imports. A real asynchronous Slang test first failed because `fixture/Surface.slangh` was absent. It passes after the fix even when the original virtual header is replaced with `#error` after scheduling. The Python build-policy fixture now extracts static module registration using the current game template's deployment target declaration instead of a removed comment; all existing host-tool selection checks remain.

CMake configuration and the affected real Debug modules, SandboxEditor/SandboxModule, AssetTool and test executables built successfully. The build retained existing third-party missing-PDB warnings. Twenty test executable invocations completed with 1,115 passing cases, three skipped cases and five previously disabled Jolt cases, with no failures. All suites below ran in full except the explicitly filtered GPU capacity cases.

| Area | Passing cases | Scope |
| --- | ---: | --- |
| SoftBody Solver / Authoring / Assets / World / Builder / Editor | 76 | 18 / 13 / 12 / 26 / 5 / 2 |
| Jolt Physics | 81 | Five existing disabled cases |
| Animation World / Module / Editor | 25 | 15 / 2 / 8 |
| Framework | 283 | Full suite |
| Editor Core / Shell / Render / Widgets | 450 | 200 / 158 / 48 / 44 |
| UI text | 26 | Full suite |
| Aurora Cook / Adaptor | 160 | 59 / 101, including four asynchronous material cases |
| Aurora Renderer Module | 9 | Two source-stripped package fixtures were not supplied and were skipped |
| Aurora Renderer GPU descriptor capacity | 5 | Six selected Vulkan/DX12 cases; the Vulkan-only descriptor-set limit case skips DX12 |

The external seal geometry/GLB, original rabbit GLB/recipe and five-rig performance matrix were supplied through the existing `SKY_SOFTBODY_*` test environment variables. All optional SoftBody asset fixtures executed without skips. This rerun covers the existing import, generation, cooking and World integration fixtures; it is not a new Android installation, mobile benchmark or interactive preview acceptance. The optional original UE per-vertex export comparison was not supplied in this run.

The CMake source-ownership suite passed 26 tests and `test_develop_configuration.py` passed all 11 without skips. Initial sandbox attempts failed on temporary-directory access or stalled at MSBuild file scanning; the successful runs used a workspace temporary directory and the authorized native build execution. Logs and XML reports are retained under `cmake-build-debug/commit-prep-20261110`.

The candidate contains 182 paths, including this active change and implementation, with only the softbody navigation/count edits selected from shared documentation indexes. The other 58 pre-existing paths remain outside the candidate. The original staged patch is preserved and the candidate patch applies to the recorded HEAD. No commit or OpenSpec archive was performed; tasks 5.4, 7.5 and 8.5 remain open.

## Archive and upstream integration 2026-11-13

- The user explicitly requested cleanup, archive, commit, rebase and push after reviewing the prepared scope and architecture.
- All 182 paths in the November 10 candidate still match their recorded worktree SHA-256 values before archive preparation. Existing build/test evidence remains applicable to that source snapshot; upstream integration requires new validation.
- The five delta domains add 25 requirements without replacing existing requirement identities: animation +1, asset-cook +8, aurora-resources +2, editor-assets +3 and physics +11.
- Tasks 5.4, 7.5 and 8.5 remain unchecked (80 of 83 complete). In particular, iOS/OHOS application/device acceptance, sustained ten-body 60 FPS and the remaining visual/editor checks are not inferred from archive status.
- The unrelated console spec edits, other research/archive files, local asset database and old demo compatibility manifest remain outside this commit.

### Rebase validation outcome

- Rebased the feature onto `origin/dev_refactor_rhi` at `d61075ff`, retaining its FrameGraph, Particle, Curve and module-manifest changes. Six textual conflicts were reconciled. No RHI production implementation or target was changed by the feature/integration fix.
- SoftBody.Builder now participates in `configs/modules_asset_tool.json`, depending on SoftBody.Assets and Aurora.Cook. Upstream manifest-driven AssetTool loading replaces the former local compile-time registration branch.
- The actual Debug SandboxModule, SandboxEditor, AssetTool, softbody/animation/Jolt/Aurora modules and all selected test executables built with the existing VS2022/third-party cache. Existing third-party missing-PDB warnings remain.
- The initial GPU test build exposed upstream direct construction of non-exported DX12 implementation classes. The existing test now creates the second device and shader through the exported backend factory and normal virtual APIs; production exports and source ownership remain unchanged. Its rejection/cause cases and the descriptor-capacity cases pass.
- Twenty-five C++ test executables completed: **1353 passed, 3 skipped, 5 existing disabled, zero failures**. This includes the original twenty-suite scope plus Aurora.Core/Pipeline, Particle.Editor/Render and Curve.Editor. The GPU selection contains eight cases (seven pass, one DX12-inapplicable Vulkan set-limit skip); Renderer.Module has two source-stripped package-fixture skips. Real rabbit/seal fixtures remain enabled, while the optional original UE vertex-export subsection was not supplied.
- Python source ownership: 26 passed; develop configuration: 11 passed with no skips; module configuration: all five passed under VS2022 and Ninja, with the final affected case rerun under Ninja. The initially failing VS test assumed a single-config deployment path; the fixture now validates the actual generated directory for each configuration. Original failure logs are retained.
- Actual AssetTool smoke in an isolated empty project loaded SoftBody.Assets and SoftBody.Builder successfully and exited zero (zero products, zero failures). This checks module loading, not a new full rabbit recook.
- OpenSpec all/strict validation: 65 passed. The five domain requirement counts are synchronized in both the README and capability map; 33 active changes remain after incorporating the five new upstream changes. Archive relative links resolve.
- Detailed logs, XML and aggregate JSON are retained in the ignored local `cmake-build-debug/commit-prep-20261113/` directory. No Android package or iOS/OHOS device acceptance was rerun in this integration pass.
