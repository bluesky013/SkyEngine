> 2026-11-13：按用户明确指令归档当前交付。任务完成 80/83；5.4（形变法线视觉验收）、7.5（三端完整应用及持续性能验收）、8.5（优化海豹交互 Generate/Save/reopen）仍未完成，保留原始未勾选状态。归档与规范同步不代表这些验收已经通过。

## 1. Scope and ownership

- [x] 1.1 Confirm four influences per vertex and seal example with user.
- [x] 1.2 Inspect Physics/Jolt, Mesh viewer, generator/document/cook and pose ownership.
- [x] 1.3 Prepare concrete module and target design.
- [x] 1.4 Obtain user approval for D1 module/target/entry changes required by AGENTS.md; user corrected interface ownership to engine/physics/softbody.
- [x] 1.5 Apply user corrections: configurable control budget with a 30-point seal default, all dynamic with finite-compliance XPBD root following; optional pins are not required by the original rabbit.

## 2. CPU solver and collision interfaces

- [x] 2.1 Add backend-neutral descriptors, constraints, collision queries and owned UUID-identified snapshots.
- [x] 2.2 Implement independent CPU XPBD lifecycle, strong failure preservation and complete destruction.
- [x] 2.3 Add deterministic before/after fixed-step stages and pin target handling; preserve pause/single-step/reset.
- [x] 2.4 Implement complete rigid-step sphere sweep/penetration queries, filters, moving surface velocity and stale-step rejection.
- [x] 2.5 Verify dynamic, fixed and root-kinematic points, compliant root following, volumes and static/kinematic contacts; explicitly reject unsupported dynamic rigid coupling. The later soft-soft extension is tracked in section 9.
- [x] 2.6 Remove the experimental native Jolt softbody path and root-follow patch; restore and rebuild the original Jolt SDK patch set.

## 3. Generator and asset pipeline

- [x] 3.1 Add pure Authoring input/output, deterministic boundary-fitted/adaptive volume proxy, constrained point insertion and quality diagnostics; replace the rejected coarse lattice default.
- [x] 3.2 Generate positive tets, unique edges/faces, stable bones and nonnegative four-weight tetrahedral binding.
- [x] 3.3 Add recipe/product schema with one metadata owner and stable source/child identities.
- [x] 3.4 Add mesh authoring/cook service in existing Aurora domain; reuse importer and product construction without new builder-to-adaptor linking.
- [x] 3.5 Build Mesh/Skin/Skeleton/SoftBody candidate from one source and publish through existing checked SaveProducts.
- [x] 3.6 Verify repeatability, constrained point preservation, coverage, rest reproduction, malformed inputs, cancellation and failed-build preservation.

## 4. Editor

- [x] 4.1 Add generic cross-DLL AssetGeneratorRegistry with duplicate-ID rejection and owned cancellable requests.
- [x] 4.2 Implement editable Mesh document and control-point picking/movement/mode authoring in the existing asset viewer.
- [x] 4.3 Register softbody binding generator through SoftBody.Editor; use reflected settings and standard actions.
- [x] 4.4 Implement complete-candidate Undo/Redo, revision guards, save/reopen and recook states.
- [x] 4.5 Verify two registered generator implementations can coexist without host-specific branches; second can be a test provider.

## 5. World and presentation

- [x] 5.1 Add SoftBody component/host consuming only Physics and pose interfaces.
- [x] 5.2 Add animation pose contribution seam without replacing global pose source.
- [x] 5.3 Preserve four-weight main/shadow rendering, dynamic bounds and complete resource identities.
- [ ] 5.4 Add validated deformation-normal contribution and analytic/visual shading checks.

## 6. Seal and verification

- [x] 6.1 Produce original detailed unbound seal and topology/visual audit.
- [x] 6.2 Run production generator with authored constrained points, save source, cook and reopen.
- [x] 6.3 Verify controlled seal runtime scenes against static and translating kinematic obstacles; report the failed 15/20-point contact cases and unsupported dynamic reaction / soft-soft CCD, edge and self-contact limits. General contact robustness and sustained device acceptance remain outside this check.
- [x] 6.4 Run relevant Authoring/Physics/Jolt/Editor/Animation/Aurora tests and actual module builds.
- [x] 6.5 Run CMakeSourceOwnership because build ownership changes; preserve existing cache and third-party paths.
- [x] 6.6 Record actual successful/failed/unverified checks, inspect diff, and report without commit/archive.

## 7. Added mobile requirements and backend review

- [x] 7.1 Record Android, iOS and the user's additional "oho" requirement, interpreted as OHOS/OpenHarmony.
- [x] 7.2 Verify PhysX native deformable GPU/CUDA restrictions, public platform support boundaries and the reference project's explicit mobile/portability motivation.
- [x] 7.3 Resolve the solver route with the user before replacing the native Jolt implementation; keep generic authoring, cooking and skeletal integration reusable.
- [x] 7.4 Establish the mobile performance budget: 60 FPS with ten simultaneous soft bodies; retain a configurable control count with a 30-point seal default.
- [ ] 7.5 Select representative Android/iOS/OHOS devices and validate complete application builds, deployment, quality and sustained performance against that budget. ARM64 object compilation and desktop results do not count as device acceptance.

## 8. Low-control cage optimization

- [x] 8.1 Add a bounded support-plane candidate in the existing Authoring target; preserve exact mandatory points and choose only between fully validated candidates within the configured control budget.
- [x] 8.2 Tighten surface-distance measurement and report separate sampled distance, certified upper bound and rest axis support offsets without including collision radius.
- [x] 8.3 Verify the actual atlas source and anisotropic regression under stricter geometric limits, plus 240 real Jolt steps checking four-weight presentation, contact compression and recovery.
- [x] 8.4 Update the seal recipe and root placement, rebuild actual modules, recook the demonstration and inspect the resulting Play scene.
- [ ] 8.5 Repeat interactive Generate/Save/reopen inspection for the optimized preview; the baseline workflow passed earlier, while the optimized provider is covered by its integration tests.

## 9. Android Release and collision benchmark extension

- [x] 9.1 Replace unsupported NDK atomic shared-pointer compiler registry with mutex-protected owned acquisition and exact-owner removal; verify owner/lifetime behavior.
- [x] 9.2 Add optional exact control budgets and small-shell generation, persist the setting, and produce all five actual-source rigs with explicit quality diagnostics.
- [x] 9.3 Add transactional CPU soft-soft batches, existing-World coordination and optional owned fixed-step metrics without new modules or targets; verify bilateral contact, rollback, lifetime and history boundaries.
- [x] 9.4 Install the Android baseline after USB permission and verify the textured seal, ground and shadow on the real device; fix all required material feature variants through normal shader cooking.
- [x] 9.5 Prepare fifteen cooked performance Worlds and matched-motion baseline instrumentation; retain raw step/application-loop/contact data and explicit failure/N/A status.
- [x] 9.6 Build and install the actual optimized Release APK; verify signing, ELF dependencies, source-free assets and benchmark readiness on the device. Correct bounded zero-step-loop sampling and animation Pending startup handling without suppressing genuine stale-identity errors.
- [x] 9.7 Execute and record the single-pair / single-body matrix followed by ten-body tests for every rig, with matched-motion baselines and device metadata: 80 primary cases (70 DONE / 10 contact failures) plus 20 separate low-impact cases (10 DONE / 10 contact failures).
- [x] 9.8 Review raw results, geometry tradeoffs, failures and thermal limits; publish validated CSV/JSON, figures, provenance and a raw-data archive without claiming unmeasured platform or sustained-frame-rate acceptance. Restore the phone to its ordinary demo with automatic benchmarking disabled.

## 10. Original rabbit replacement

- [x] 10.1 Confirm that four bones means at most four influences per vertex, and read/export the saved original rabbit without modifying the UE assets.
- [x] 10.2 Preserve the full saved body, corner attributes, original eye geometry and three material slots; audit the converted geometry and winding.
- [x] 10.3 Add supplied center-fan generation, displacement embedding and constant attachment anchors in existing targets, with version-one compatibility and explicit normal policy.
- [x] 10.4 Run the actual rabbit through the production importer, generator and Mesh/Skin/Skeleton compiler; verify 15 dynamic controls, 24 tetrahedra, 50 edges, 16 bones and original four-weight agreement.
- [x] 10.5 Cook and inspect the original rabbit demonstration, run actual-parameter physics regressions, and record runtime differences from the UE gameplay controller.
- [x] 10.6 Build and install the independent Android Release demonstration, inspect rendering and retain verification records. Keep the earlier seal measurements separate from rabbit results.

## 11. Rabbit collision performance matrix

- [x] 11.1 Produce and validate exact 30/25/20/15/10-control rigs from the original rabbit with at most four influences, explicit geometry-quality diagnostics and mass/follow normalization; preserve the original 15-point cage as a reference.
- [x] 11.2 Prepare matched-motion baselines and contact Worlds for two rabbits, one rabbit versus a vertical static/kinematic wall, and ten-body workloads; retain genuine contacts, projection peaks and separate native-rigid costs.
- [x] 11.3 Build and validate the independent Release benchmark artifact, install it and smoke-test every collision profile before full collection.
- [x] 11.4 Run the pair/single peak stage before the ten-body stage for all five rigs, keeping the previous 120-warmup/600-sample/60-Hz/four-iteration/0.2-m-per-second per-channel initial protocol and explicit failure/N/A records.
- [x] 11.5 Publish rabbit-only raw data, baseline deltas, contacting-step peak/P95/P99, solver buckets, geometry tradeoffs and device/thermal provenance. Do not substitute earlier seal results or equate changed wall/ground fixtures.
- [x] 11.6 Disable automatic benchmarking after collection, restore the ordinary rabbit demonstration and report actual validation limits without commit/archive.

## 12. Ordinary two-rabbit collision and lighting

- [x] 12.1 Make the current Android project's default World contain two inward-facing rabbits and a static ground, with configurable continuous root-driven approach/hold/separation in its existing Launcher target; preserve the frozen benchmark artifact.
- [x] 12.2 Diagnose missing environment lighting and display encoding; author reproducible linear diffuse/specular/BRDF IBL assets, preserve GGX mip levels and use the existing HDR/tone-map path without fixed material fill.
- [x] 12.3 Correct the KTX loader's non-square terminal-mip dimensions, cover KTX1/KTX2 through image writing and runtime scene conversion, and rebuild the real cook module/tool/tests.
- [x] 12.4 Recook and verify the complete Release package, install it and inspect repeated real soft/ground contact and the brighter default scene; retain actual failures and deployment evidence.

## 13. Visible rabbit contact

- [x] Fit the ordinary demonstration's nose contact to the displayed mesh and bind both actors to its cooked rig, mesh, skin and skeleton.
- [x] Verify a separated, loaded and released pose using the skinned geometry from the installed Release APK and independently inspect the on-device silhouettes.

## 14. Back-to-back demo and live Android collision overlay

- [x] Author a 15-point rear-fitted demonstration rig, face both rabbits outward and calibrate the longer, faster approach against the production solver.
- [x] Expose owned, read-only published control positions, distance constraints, surface triangles and particle radius through the existing Physics soft body interface. Cover interpolation, topology, lifetime and failed-step behavior.
- [x] Verify the installed Release's rear surface contact, approach speed and recovery, and operate the on-screen debug switch on the device.

## 15. Jelly recoil after rear impact

- [x] Compare repeat impacts through the production solver, measure deformation relative to the body, and tune the dedicated rabbit material and root motion.
- [x] Compare published skinned poses on the same Android phone, verify rear contact and recovery, and restore the ordinary demo for visual inspection.

## 16. Interactive Android jump

- [x] Add an ordinary-mode jump button for both grounded rabbits. Stage one request during a collision and preserve the existing dynamic rig through takeoff and landing.
- [x] Check repeated jumps during the back-impact cycle with the production solver and fit the camera to the skinned silhouettes.
- [x] Record two grounded jumps and the intervening airborne rejection in the ordinary view, then a jump in the debug view. The installed 120 Hz Release on the Xiaomi is recorded in `rabbit-impact-device/final-jump/`.

## 17. Editable material HUD and five rig sizes

- [x] Read active body parameters and apply a validated patch to selected instances together. Cover snapshot continuity, bad input and shared-rig isolation in the existing solver and World tests.
- [x] Supply the five complete cooked rig variants and stage both rabbit actors together for in-place switching at grounded separation.
- [x] Verify real parameter readback, reset, each rig count and a jump on the installed Release. The final 120 Hz Xiaomi run is in `rabbit-impact-device/final-controls-srgb/release-check.json`. The earlier 50d24707 run remains in `jump-hud-device/final-2/release-check.json`.

## 18. Match the original XPBD pair impact

- [x] Run the original pair at 120 cm/s, compression 1 and rebound 0.5. Retain the original runtime frames and measurements.
- [x] Replace the timed default horizontal trajectory with a depth- and velocity-driven pair. Check repeated contact against the production solver and the five demo rigs.
- [x] Compare the prior and revised Android Release poses and ordinary recordings with the original pair. Restore the default scene and retain the signed package and evidence.
- [x] Reproduce the repeated-impact stall on the Redmi and cover its residual with a deterministic solver regression. Verify 30 actual pair contacts and releases with two intervening jumps in a single session on the replacement Xiaomi.
