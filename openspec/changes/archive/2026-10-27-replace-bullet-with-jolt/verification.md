# Verification

## Archive and standalone commit boundary (2026-10-27)

The user authorized archiving the Jolt adaptation separately from the later material/Collision assets and streaming work. This change was archived with all six capability deltas synchronized. The commit remains subject to the repository's message-and-file-list confirmation rule.

An isolated source snapshot was assembled from HEAD `ba105c592d39217c994e32959bff22a59882947a` plus only this change. It excludes JoltPhysics.Builder, asset codecs/handlers, prepared-shape jobs, WorldStreaming and the active add-physics-collider-authoring change. Shared files were split without replacing the working tree or the existing staged changes. The snapshot was configured with the existing VS 2022 generator and cache's SKY options / `D:/Code/Engine/sky_3rd/Win32`; its temporary build directory leaves the main cmake-build-debug untouched.

- Related Debug build passed: JoltPhysicsTest, both Jolt modules, FrameworkTest, EditorCoreTest, EditorShellTest, SandboxProjectTest, SandboxModule, SandboxEditor, AssetTool, RecastModuleTest, TerrainPhysicsTest and CompressionModule.
- Standalone snapshot tests passed: Jolt 41, Framework 207, EditorCore 184, EditorShell 144, targeted Sandbox integration 7, Recast 2 and Terrain 2. Real Jolt DLL lifecycle passed two sessions and retained-world checks.
- Python package tests: 8 passed, including native source-free product auditing and missing collision-product rejection. The first isolated run omitted CompressionModule from its build targets and failed to load that DLL; building the required module and rerunning the package suite passed. Both logs are retained.
- All six synchronized main specs passed strict validation. The candidate diff passed whitespace checks excluding the vendor patch's required unified-diff context indentation; `git apply --stat` validates that patch's syntax.
- Evidence lives under `build_3rd/jolt-adapter-commit/`: `build.log`, `compression-build.log`, per-suite XML/logs, `module-lifecycle.log`, `python-tests-final.log`, `commit-review.md` and `candidate.patch`. This verifies the independent migration slice, rather than borrowing the later 58-test streaming result.

## Strict integration review (2026-10-27)

- The subsequent [World / editor / Jolt integration review](review-2026-10-27.md) found 6 P1 and 5 P2 issues. That review initially blocked integration acceptance. The corrections and current verification are recorded in the new section below; older checks retain their original scope.
- The existing 30 Debug Jolt tests passed again in an isolated executable linked from the existing test objects/libraries. Seven focused expected-behavior repros failed, and a separate event-retention diagnostic confirmed 600 queued events after 600 steps with one persistent contact. Logs and exact coverage limits are in the review.
- This review changed no production implementation. The demonstration editor was closed for the repro build; the scene files were preserved.

## Root-cause repair verification (2026-10-27)

The user requested fixes at the source of all R1–R11 findings. The corrected contracts and per-finding causes/evidence are recorded at the end of [the review](review-2026-10-27.md) and in design.md. All eleven findings are closed within the Windows validation scope below; no commit/archive was performed.

| Current check | Result |
|---|---|
| Debug FrameworkTest | 207 passed |
| Debug EditorCoreTest | 184 passed |
| Debug EditorShellTest | 144 passed |
| Debug SandboxProjectTest filter `WorldEditorState.*:WorldSkinDefaultsTest.*:SandboxCreationActions.*` | 7 passed |
| Debug / Release JoltPhysicsTest | 41 / 41 passed |
| Debug / Release `JoltPhysicsTest --module-lifecycle` | Passed: two real DLL sessions, one metadata owner, retained live worlds |
| Debug RecastModuleTest / TerrainPhysicsTest | 2 / 2 passed |
| Python `test_platform_package_configs.py` | 4 passed |
| Python `test_animation_package.py` | 4 passed, including real product audit for collision-only mesh and missing-product rejection |
| Debug SandboxEditor, existing SceneVerification project, 30 frames | Exit 0; log confirms editor shell, Jolt modules, Vulkan swapchain and Renderer host stopped |
| Strict OpenSpec validation and scoped `git diff --check` | Passed |

Builds preserved cmake-build-debug, the VS2022 generator and existing 3RD_PATH. Targets included FrameworkTest, EditorCoreTest, EditorShellTest, SandboxProjectTest, JoltPhysicsTest, both Jolt modules, SandboxModule, SandboxEditor, AssetTool, RecastModuleTest and TerrainPhysicsTest. Release rebuilt JoltPhysicsTest and both Jolt modules plus their dependencies. Newly added source files were included by reconfiguring first. The editor was closed for builds.

New tests cover cooked interleaved vertex/bulk conversion and both index widths; config history after vector growth/reload; missing provider/config/factory rollback; component UUID type rejection; attachment failure and retry; failed property commands and transaction redo rollback; stable body/constraint handles and velocity across mass/shape edits; static/kinematic input and completed-step dynamic output; nested collider asset references; next-Play config/disable semantics; generic component requirements; real creator action registration/removal with code leases; bounded event retention and ordered multi-listener trigger delivery.

Logs are under `build_3rd/`: `jolt-root-final-build2.log`, `jolt-root-validation-build.log`, `jolt-root-consumer-build.log`, `jolt-root-release-build.log`, `framework-root-full.log`, `editor-core-root.log`, `editor-shell-root.log`, `editor-world-root.log`, `jolt-root.log`, `jolt-root-release.log`, `jolt-root-dll.log`, `jolt-root-release-dll.log`, `jolt-root-recast.log`, `jolt-root-terrain.log`, `jolt-root-package-tests.log`, `jolt-root-native-audit.log`, `jolt-root-editor-smoke.log`, and `jolt-root-openspec.log`. Matching gtest XML files preserve suite/count/failure evidence.

Environment qualifications:

- Restricted Windows sandbox runs of file replacement/rename tests failed with Access Denied. The tests were rerun with normal native permissions and passed; these initial failures are not represented as code failures or silently omitted.
- `third_party.py --check` reports Jolt v5.6.0 OK. The global preflight still reports six pre-existing metadata/version issues: stale miniaudio/enet; missing metadata for vulkan_headers, nanosvg, openssl and cpython. No output-missing package was reported. The shared third-party tree was not rewritten for unrelated packages.
- Debug/Release Jolt execution is current. Full Release editor tests and non-Windows platform execution were not performed. The bounded editor smoke validates startup/shutdown; it does not replace the earlier manual scene simulation session.
- No collision fitting/decomposition UI or independent collider asset pipeline was implemented in this repair; those remain in add-physics-collider-authoring.
- Active source/build/package scans remain free of Bullet; its shared historical third-party cache remains excluded from active dependencies. No new throw/try/catch/dynamic_cast or exposed Jolt SDK types were introduced.

## Scope and environment

- Implemented 2026-10-26/27 after user confirmation and the explicit adaptor + render revision. No Jolt core target remains. No commit or OpenSpec archive was performed.
- Existing `cmake-build-debug`, generator `Visual Studio 17 2022`, `3RD_PATH=D:/Code/Engine/sky_3rd/Win32` were preserved. Final cache: `SKY_BUILD_JOLT=ON`, no `SKY_BUILD_BULLET` entry.
- Jolt v5.6.0 revision: `e77f175595e64cb44218cc9d9d56fc365ad0e36a`. SDK Debug/Release were built with manifest options and checked against the importer. MIT notices were installed, deployed and staged.
- Unrelated pre-existing working-tree changes were preserved.

## Build commands and results

The editor was closed before configuring/building. Native builds used escalated toolchain access after sandbox MSBuild could not complete.

```powershell
python python/third_party.py -p Win32 -o D:/Code/Engine/sky_3rd -t jolt -j 8 -f
python python/third_party.py -p Win32 -o D:/Code/Engine/sky_3rd --check
cmake -S . -B cmake-build-debug -U SKY_BUILD_BULLET
cmake --build cmake-build-debug --config Debug --parallel 8 --target JoltPhysicsTest JoltPhysicsModule JoltPhysicsRenderModule PhysicsTest TerrainPhysicsTest RecastModuleTest FrameworkTest EditorCoreTest EditorShellTest SandboxModule SandboxEditor
cmake --build cmake-build-debug --config Release --parallel 8 --target JoltPhysicsTest JoltPhysicsModule JoltPhysicsRenderModule
```

- SDK builds and all listed native target builds passed. Logs: `build_3rd/jolt-sdk-rebuild.log`, `jolt-integration-build.log`, `jolt-build-release.log`; final backend/test rebuilds: `jolt-final-debug-build.log`, `jolt-final-release-build.log`.
- Global dependency preflight is **not green**: Jolt is `ok v5.6.0`; pre-existing miniaudio/enet pins are stale, vulkan_headers/nanosvg/openssl/cpython metadata is missing. No output-missing package was reported. The script checks the whole manifest even with `-t jolt`; a successful targeted check is not claimed. Log: `build_3rd/jolt-third-party-check.log`.
- Physics-disabled validation passed in the same cache: `cmake -S . -B cmake-build-debug -DSKY_BUILD_JOLT=OFF`, build `PhysicsTest`, then restore `-DSKY_BUILD_JOLT=ON` and reconfigure. Logs: `jolt-disabled-configure.log`, `jolt-disabled-build.log`, `jolt-final-configure.log`.
- Scope is Windows x64 and dynamic plugin deployment. Other operating systems, shipping static-module packaging and performance benchmarks were not exercised.

## Tests

| Command / check | Result |
|---|---|
| `output/bin/Debug/JoltPhysicsTest.exe --gtest_color=no` | 29 passed |
| `output/bin/Release/JoltPhysicsTest.exe --gtest_color=no` | 29 passed |
| Debug and Release `JoltPhysicsTest.exe --module-lifecycle` | Passed in fresh processes with actual DLLs |
| `output/bin/Debug/FrameworkTest.exe` | 205 passed |
| `output/bin/Debug/EditorCoreTest.exe` | 179 passed |
| `output/bin/Debug/EditorShellTest.exe` | 144 passed |
| `output/bin/Debug/TerrainPhysicsTest.exe` | 2 passed |
| `output/bin/Debug/RecastModuleTest.exe` | 2 passed |
| `output/bin/Debug/PhysicsTest.exe` | Exit 0; currently 0 tests, only a link/startup check |
| `python -m unittest discover -s python/tests -p test_platform_package_configs.py` | 3 passed, including Jolt notice staging |

Jolt coverage includes stale/foreign/type handles; body and native-update capacity failure; transactional options; retained worlds after unregister/restart; COM, pivots and scale; all shape families; rectangular heightfields/up axes/tile seams; U16/U32 mesh strides and bad indices; high-bit bilateral filters; sleeping/removal events; materials and symmetric restitution; five constraint kinds and dependent teardown; distance length and slider drive/limits; CCD; character displacement, resize, character collision, stairs/slope/platform; snapshot validation; Tick/Pause/SingleStep; collection serialization; scaled inertia; and debug category separation including AABB combinations.

The DLL test performs two manager sessions with one metadata owner, creates configured Physics subsystems, checks gravity/fixed-step behavior and deterministic-config rejection, unregisters services, steps retained worlds after shutdown, and verifies the cross-DLL render provider is cleared. This exercises SDK and code leases with real modules.

Logs: `build_3rd/jolt-tests-debug.log`, `jolt-tests-release.log`, `jolt-dll-lifecycle-debug.log`, `jolt-dll-lifecycle-release.log`, `<TargetName>.log`, `jolt-platform-config-tests.log`.

## Editor and package smoke

```powershell
python python/package_project.py create --dir build_3rd --name JoltMigrationSmoke
output/bin/Debug/SandboxEditor.exe --console --frames 8
output/bin/Debug/SandboxEditor.exe --project D:/Code/Engine/SkyEngine/build_3rd/JoltMigrationSmoke/JoltMigrationSmoke.skyproj --console --frames 12
python build_3rd/verify_jolt_package.py
```

- Both bounded editor processes exited 0. The editor entry point also returns 0 after initialization failure, so these exit codes alone do not prove successful startup. Interactive validation is recorded below; new authoring tools remain design-only.
- Generated project and staged shipping module lists contain JoltPhysicsModule/JoltPhysicsRenderModule and no Bullet. `stage_configs` ran on this generated project; staged and Debug/Release deployed notices matched the plugin notice byte-for-byte. Log: `build_3rd/jolt-package-staging.log`.
- SandboxModule and SandboxEditor were rebuilt. Obsolete generated Develop/Release module lists and copied Debug template/config locations were refreshed.
- Broader `test_native_package_artifacts.py` was attempted: the sandbox run hit temp-directory permissions. Retrying with workspace TEMP/TMP and native permissions yielded 15 tests, 2 failures, 4 errors, 1 skipped. Failures are POSIX path expectations on Windows; errors concern Apple fixture executable discovery and unavailable symlink privileges. These are outside the changed config-staging path. Log: `build_3rd/jolt-package-tests-workspace-temp.log`. Full cross-platform package acceptance is not claimed.

## Existing-project rigid-body demo (2026-10-27)

- Added `assets/jolt-rigid-demo.world` to the existing `output/scene-verification/SceneVerification.skyproj` project, preserving `assets/main.world`. The scene contains 18 dynamic boxes, 3 dynamic spheres, static ground and a ramp, lighting and a runtime camera.
- Generated and cooked `assets/rigid-sphere.obj`; its source UUID is also the default mesh product. The batch cook produced the required mesh products, but reported four unrelated `.world` cook failures (two worlds across two targets) because no world builder is registered. The editor opens those world source files directly; whole-project cook success is not claimed.
- Interactive Debug editor validation confirmed scene loading, visible sphere meshes, Play camera switching, falling bodies settling against the ground/stack/ramp, Pause, and Stop followed by Play resetting the initial arrangement. The viewport was enlarged and the editor was left open for inspection. Shadows are disabled in this demo to keep the existing renderer's shadow artifacts out of the physics preview.
- The initial scene fix made `PhysicsSystem::OnAttachToWorld` bind existing body components. The root-cause repair above supersedes that feature-specific scan with detached loading and unified World attachment after subsystem creation. Added `PhysicsComponentTest.LoadedActorsBindWhenPhysicsSubsystemIsBuilt`, covering serialization, deferred registration and subsequent gravity.
- Rebuilt Debug `JoltPhysicsTest`, `JoltPhysicsModule`, `SandboxModule`, and `SandboxEditor`; all 30 Debug Jolt tests passed. Release's 29-test result above predates this follow-up and was not rerun. Logs: `build_3rd/jolt-scene-build.log`, `jolt-scene-tests.log`, and `jolt-demo-cook.log`.

## Cleanup and source audit

- Deleted all tracked `plugins/bullet` files and `cmake/thirdparty/Findbullet3.cmake`; removed bullet3 manifest entry, plugin/build options, runtime/template entries and active examples.
- Deleted identified generated Bullet DLL/PDB/test artifacts and obsolete Bullet/Jolt-core build directories after verifying workspace paths.
- Active scans of `configs engine plugins cmake python AGENTS.md .opencode/skills` excluding binary DB/pyc are clean. Deployed JSON/config lists and filenames were also scanned. Historical archives and comparisons retain historical references.
- External shared `sky_3rd` caches were preserved and are no longer active dependencies. Repository plugin and third-party integration are removed.
- Audited no new throw/try/catch/dynamic_cast, no parent-relative public include or non-ASCII C++, no Jolt types/headers in public plugin or engine physics headers. Native headers are private; render does not depend on the SDK/adaptor.
- Reviewed metadata ownership, Environment-backed shared state, code leases, shutdown symmetry and dependency direction. Scoped `git diff --check` passed, with line-ending notices only.

## Delivered limits and follow-up

- Fast mode only; Exact/deterministic requests are rejected. Snapshots cover rigid-body state, not complete solver/character rollback.
- Concave geometry requires static bodies. Heightfields use triangle-grid cooking: rectangular grids, axes and seams are preserved at a higher memory cost than native compressed heightfields.
- Separate static/dynamic friction and anisotropic damping are rejected. Capacities are fixed per world; native update capacity failure faults the world after jobs finish.
- External projects must rename BulletPhysicsModule/BulletPhysicsRenderModule to JoltPhysicsModule/JoltPhysicsRenderModule and SKY_BUILD_BULLET to SKY_BUILD_JOLT. No compatibility alias remains.
- New collision asset/UI/generation/mesh-cook/CoACD tooling is a **design-only** proposal in `../add-physics-collider-authoring/`, not an implemented feature of this migration.

## OpenSpec

- `openspec validate replace-bullet-with-jolt --strict`: passed.
- `openspec validate add-physics-collider-authoring --strict`: passed.
- Ready for review within the validation scope above. Commit/archive remain separate user-confirmed actions.
