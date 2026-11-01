# Verification — 2026-10-30

## Native environment

The existing cmake-build-debug cache was preserved: Xcode, Debug, MacOS-x86, SKY_BUILD_TEST=ON, SKY_BUILD_SANDBOX=ON. Its 3RD_PATH remains the existing build_3rd/MacOS-x86 tree.

Third-party preflight reported available outputs but stale metadata for Recast, Jolt, ENet and Slang. Native evidence below uses those existing installed libraries; it is not validation of a freshly rebuilt pinned dependency set.

## Production targets

CMake configuration and source-ownership validation succeeded. The real production targets were built, including RecastNavigation (backend/shared/runtime owners), SkyNavigation.Builder, RecastNavigation.Editor (render/editor static owners), NavigationTerrain, SandboxModule and SandboxEditor. Tests link the production implementation owners.

## Automated tests

The selected CTest run passed 12/12 executables/checks:

- CMakeSourceOwnership
- CoreTest and FrameworkTest
- EditorCoreTest and EditorShellTest
- NavigationTest and RecastModuleTest
- RecastNavigationTest
- RecastNavigation.RenderTest and RecastNavigation.EditorTest
- TerrainPluginTest and NavigationTerrainTest

The standalone source-ownership suite also passed 26 tests (one existing skip) with the Xcode generator. Its first sandboxed CTest invocation failed to detect a C++ compiler; rerunning with access to Xcode's compiler/cache paths passed. No ownership-check bypass was added.

Backend tests exercise actual Recast generation across tiles, projected endpoints, foreign filter data, disconnected regions, incremental preservation, full/tiled round trips, invalid setup/provider coverage, corrupt payload rejection/recovery and owned streaming data with a nonzero bounds origin.

Runtime tests cover all manifest layers, coordinate budgets, failed-layer rollback/retry, settings validation, missing product failure, duplicate factory ownership, detach/query cancellation and streaming mutation invalidation. Module tests include reload, failed registration preserving another owner, invalid world settings and retained objects remaining usable after shutdown. Core task tests verify release with retained dependency handles and restarting completed tasks.

Render packet tests cover owned geometry, updates/releases, retirement before recreation, scene-slot changes and renderer epochs. Editor tests cover deferred generation, closing the active world, play-mode disabling, action cleanup and permanently deactivated old sessions.

## Native smoke checks

SandboxEditor --frames 3 --console completed successfully with Metal on Intel Iris Plus Graphics 655. RecastNavigation, RecastNavigation.Editor and SkyNavigation.Builder loaded; the renderer stopped normally.

A temporary copied editor/config added a nonexistent module gated by disabled SKY_BUILD_PYTHON and referenced it from SandboxModule. The same bounded startup succeeded without attempting to load that disabled dependency, verifying optional editor dependency filtering.

## Limits

The navigation scene and disk-product-to-runtime binding were subsequently verified as described below. The independent navigation-terrain-build-tests matrix remains separate. Box primitive colliders alone are still outside the navigation geometry collection path; this scene uses actual independent TriangleMesh collision assets.

No commit or OpenSpec archive was performed.

## Real scene / disk products / editor verification

The user-requested project is `output/NavMeshValidation/NavMeshValidation.skyproj`. Its authoring world contains a 24 x 16 m ground, a 2 x 3 x 6 m wall, a disconnected 4 x 4 m island, query markers and an overview camera. Render and independent collision geometry match. The `.navmesh` source uses 0.25/0.2 m cells, 8 m tiles and a 1.6 m high, 0.35 m radius agent.

The initial real cook reported success but exported zero tile layers; the new disk-product test reproduced that failure. The scene exposed and now covers:

- Deferred body attachment: offline cooking explicitly polls component readiness and drains loader work without waiting on its own cook pool; editor generation polls across frames without blocking or simulating.
- Collision geometry: the codec now constructs the whole-mesh view and bounds needed by the navigation consumer.
- LZ4: cook/editor/runtime hosts load the compression provider; missing-provider generation reports an explicit error.
- Actual debug material: canonical `gMaterial.tint` is updated and the translucent debug draw sorts after ordinary geometry.

After these fixes, the existing AssetTool produced 11 products with zero failures. The cooked navigation product has 15 tile layers and 39 triangles. The new test in the existing RecastModuleTest loads the actual product with ProductsOnly and creates the navigation service through its `navMeshAsset` config. It verifies the five-point detour from (-8,0,0) to (8,0,0), length 17.6001797 m, rejects the disconnected destination (20,0,0), and verifies the asynchronous query. Its output is `validation/paths.json` and `validation/navmesh.obj` inside the project.

A second copy containing only source/config files, with no project products, also cooked successfully on the first pass and passed the same disk-product test. Both result JSON files matched exactly, ruling out an old project product as the source of success.

The actual native editor was opened with the project. Build Navigation was invoked on the authoring world and produced visible green walkable geometry. The runtime world was then opened and played through the overview camera; the green ground, excluded wall footprint and disconnected island were visually inspected on Metal. A local `.app` wrapper identifies the existing built editor to the UI automation service; it does not introduce another engine tool or target.

Relevant regressions passed: FrameworkTest (238 passed, 14 existing GUI skips), JoltPhysicsTest (62 passed, five disabled tests), RecastNavigationTest (9 passed), RecastNavigation.RenderTest (4 passed), RecastNavigation.EditorTest (6 passed), and the explicit scene product test (passed for the project and for the clean copy). The Xcode source-ownership suite passed 26 tests with one existing skip. Saved logs and the cooked-data PNG preview are under the project's `validation/` directory.

## Checkpoint review — 2026-11-01

The current Xcode/Debug cache and MacOS-x86 3RD_PATH were preserved. No editor process was running before the build. Configuration/source ownership and the affected real libraries, backend/runtime/editor modules, builder, terrain bridge, SandboxModule and SandboxEditor built successfully. A final targeted rebuild also passed after the idempotent removal fix and interface contracts.

The first CTest pass completed 13/13 groups: CMakeSourceOwnership, CoreTest, FrameworkTest, JoltPhysicsTest, EditorCoreTest, EditorShellTest, NavigationTest, RecastModuleTest, RecastNavigationTest, RecastNavigation.RenderTest, RecastNavigation.EditorTest, TerrainPluginTest and NavigationTerrainTest. After the last code changes, all six affected navigation groups were rerun and passed. The real disk-product scene test was separately rerun with `SKY_NAVIGATION_SCENE_PROJECT` pointing to the existing validation project and passed its synchronous/asynchronous detour and disconnected-island checks. This optional scene case is skipped by default without that project environment variable.

New checks cover:

- Independent real Detour contexts: a second query finishes while a first real QueryPath is held inside its shared read section; four readers repeatedly verify isolated forward/reverse results.
- Concurrent query/tile replacement under shared-reader/exclusive-writer protection. The stress test is not proof of writer fairness or a frame-time bound; the held-reader pause is in filter extraction, not an instrumented measurement of two A* loops.
- Submission-time filter snapshots, stable completed results after late Cancel, and cancelled-but-running jobs retaining admission slots.
- Permanently failed coordinates yielding to fresh healthy work, bounded backoff, failed rollback retried before reinstall, and cleanup after focus leaves the coordinate.
- Idempotent removal of absent coordinates, including a real first-layer compression failure that recovers after LZ4 service restoration.
- Small payloads declaring huge tile/raw/full-data sizes, duplicate layers, invalid mode/version, truncation and invalid saves; rejection happens before declared large allocations.
- Terrain bridge owning the last WorldPtr: provider unregistration occurs before subsystem destruction, and repeated Shutdown is harmless.

`SandboxEditor --frames 3 --console` completed with Metal, loading RecastNavigation, RecastNavigation.Editor, SkyNavigation.Builder and SandboxModule, and stopping its renderer normally. OpenSpec strict validation passed for the checkpoint and the separate common-IO design. Code formatting used the existing Apple clang-format 17 and source checks found no newly introduced throw/catch/dynamic_cast.

Third-party preflight still reports stale metadata for Recast, Jolt, ENet and Slang, with all outputs present. These results validate the installed local libraries, not a fresh pinned-dependency rebuild or Win32 execution. No query throughput/speedup, lock-free behavior, writer-latency bound or true disk tile paging is claimed.

Logs: `/tmp/skyengine-navigation-checkpoint-{configure,build,tests,final-build,final-tests,scene,editor}.log`. The exact proposed commit paths are listed in commit-files.txt; the common IO design is excluded. No commit or archive was performed.

The final format pass covered all 75 live C++ files in the proposed scope: ASCII/new prohibited-pattern checks and Apple clang-format --dry-run --Werror passed. Existing unrelated project-creation exception handlers were not changed. Format-only edits were rebuilt in JoltPhysicsTest, SandboxModule and SandboxEditor; JoltPhysicsTest and EditorShellTest were rerun and passed (2/2). Corresponding logs use the `format-build` and `format-tests` suffixes. The staged no-rename manifest was checked exactly against commit-files.txt: 125 paths, with the entire common-IO proposal excluded.

## Framework builtin compression revision — 2026-11-01

This revision supersedes the earlier CompressionModule host-loading arrangement. Framework now owns the default stateless LZ4 codec behind CompressionManager, initialized by the application host before module loading. Production calls retain acquired provider handles. The codec implementation has one Framework compilation owner; plugin source, target, registry entry and host configurations were removed. SKY_BUILD_COMPRESSION is an ordinary default-ON foundation option, not a module-loader option. Product encoding is unchanged.

The preserved primary Xcode/Debug cache configured successfully, and all configured targets built successfully, including the real libraries, runtime/editor/builder modules, AssetTool and SandboxEditor. The final CTest pass completed 14/14 groups: CMakeSourceOwnership, CoreTest, FrameworkTest, Content.BuilderTest, JoltPhysicsTest, EditorCoreTest, EditorShellTest, NavigationTest, RecastModuleTest, RecastNavigationTest, RecastNavigation.RenderTest, RecastNavigation.EditorTest, TerrainPluginTest and NavigationTerrainTest.

Framework's six compression cases cover default availability, override rejection/restoration, acquired object/code-lease lifetime and destruction order, disabling new acquisition, native round trips/error boundaries/empty payloads and independent parallel calls. This validates local lease behavior; no dedicated real-DLL-unload test was performed. The borrowed GetCompressor compatibility API still requires quiescent provider reconfiguration; production operations use AcquireCompressor.

An isolated Xcode build at `/tmp/skyengine-framework-compression-off`, with SKY_BUILD_COMPRESSION=OFF and the same installed 3RD_PATH, configured and built the real FrameworkTest target. Four interface/ownership tests passed; the two native-codec cases skipped as expected. AssetManagerTest.CompressionTest separately passed. Its binaries stayed in the temporary build output, preserving the main cache and enabled binaries.

A fresh source/config-only scene copy at `/tmp/skyengine-nav-framework-compression` was cooked with `AssetTool --project /tmp/skyengine-nav-framework-compression --engine /Users/lizhen/Program/SkyEngine --cook-all --platform macos`: 11 products, zero failures. An initial invocation with `--platform common` selected no preset and produced zero products; that result was not accepted as cook evidence. The macos preset correctly selects the common product bundle.

The stale generated Debug `libCompressionModule.dylib` was moved to `/tmp/skyengine-obsolete-compression-module` before final runtime checks. With the old DLL absent, the explicit RecastSceneTest passed using the new disk products: 15 layers, 39 triangles, five detour points, length 17.6001797 m, synchronous/asynchronous success and disconnected-island rejection. `SandboxEditor --frames 3 --console` also completed on Metal without loading CompressionModule.

The final source pass covered all 86 live C++ files in the revised proposed scope: ASCII, added throw/catch/dynamic_cast checks and Apple clang-format --dry-run --Werror passed. Strict OpenSpec validation and the exact staged manifest check passed. The revised manifest contains 146 paths (M 62 / D 26 / A 58), including this change's specs and implementation; the entire independent add-common-async-io proposal remains excluded. No commit or archive was performed. The installed-dependency and platform limits recorded above still apply.

Logs use `/tmp/skyengine-framework-compression-` with suffixes `configure`, `build`, `all-build`, `tests`, `off-configure`, `off-build`, `off-tests`, `off-product`, `cook-macos`, `scene` and `no-module-editor`, followed by `.log`. The new scene artifacts are in its `validation/` directory.

The user explicitly authorized the reviewed commit on 2026-11-01. Before committing, the 146-path staged manifest and whitespace checks were verified again; task 6.8 now records that authorization. Archive readiness is being assessed separately, without applying main-spec changes or moving the change directory.

Archive assessment found two stale main-spec dependency lists: the offline builder and backend now consume `RecastNavigation.Shared.Static`. Their existing delta files now include full MODIFIED requirements for these boundaries, preserving separation from runtime/editor/render targets. These documentation corrections are included in the same implementation commit. All 32 tasks and all four workflow artifacts are complete; strict change validation passed. Main-spec synchronization and archive still require explicit user confirmation.

## Authorized archive and publication — 2026-11-01

The user explicitly authorized archive, commit, rebase and push. `openspec archive aurora-navigation-integration --yes` completed with validation enabled, synchronizing nine main specs (20 added, five modified and four removed requirements) and moving the complete change to `openspec/changes/archive/2026-11-01-aurora-navigation-integration`. The CLI reported non-blocking proposal-format/size suggestions; it accepted all delta specs and complete tasks. This archive and the main-spec updates are folded into the implementation commit. The independent untracked `add-common-async-io` proposal remains outside it.

The implementation/archive commit was rebased onto `origin/dev_refactor_rhi` at `ab7ecc12` (HarmonyOS runtime and mobile native text input). The sole conflict in AssetTool registration preserves upstream audio/terrain build-option gates and navigation's builder availability guard plus explicit Recast backend dependency. A range-diff and independent read-only review verified the merged framework/platform changes and compression ownership. All 12 independent IO proposal files match their pre-rebase SHA-256 hashes.

Using the preserved Xcode/Debug cache and installed MacOS-x86 dependencies, post-rebase configuration and the build of all configured targets passed, including AssetTool, RecastNavigation, SkyNavigation.Builder, RecastNavigation.Editor, SandboxModule and SandboxEditor. All 19 selected CTest groups passed: the previous 14 groups plus Aurora.RHITest, Aurora.CoreTest, Aurora.ShaderTest, Aurora.AdaptorTest and Aurora.Renderer.ContractTest. All nine synchronized main specs passed strict validation; the new Aurora integration spec's generated Purpose placeholder was replaced with its actual scope.

A new source-only project copy at `/tmp/skyengine-nav-post-rebase` cooked 11 products with zero failures through the merged AssetTool. Its explicit disk-product scene test passed synchronous/asynchronous routing and disconnected-island rejection, reproducing 15 layers, 39 triangles, five path points and length 17.6001797 m. The three-frame Metal editor smoke also completed normally. No additional production code changed after these checks. The final no-rename commit manifest contains 159 paths, including main specs and archive, with the independent IO proposal excluded.

Post-rebase logs: `/tmp/skyengine-navigation-publish-{configure,build,tests,cook,scene,editor}.log`. The dependency/platform limitations noted earlier still apply; the incoming HarmonyOS/device implementation was not executed on a HarmonyOS device during this navigation publication.
