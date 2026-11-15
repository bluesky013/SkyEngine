# Verification

## Environment and scope

Verified on macOS x86_64 using the existing Xcode Debug configuration in `cmake-build-debug`, preserving `3RD_PATH=/Users/lizhen/Program/SkyEngine/build_3rd/MacOS-x86`. Windows/VS2022 and Linux were not built in this session.

This record covers source identity/mutations, material preparation and independent resolution, preview default injection, writable DDC, and the related obsolete-tool cleanup. The target/metadata/editor-policy changes in `architecture-follow-up.md` remain proposed and require separate approval; they are not claimed as implemented. Existing hover-tooltip and project-creation work remains in its own active changes.

## Build

CMake configure and affected real libraries/modules succeeded, including Framework, Aurora Adaptor/Cook, Recast render/editor, SoftBody builder, SandboxModule, SandboxEditor and AssetTool. All listed test binaries below were rebuilt before their final runs. Editor shell changes were verified through SandboxModule, not only the launcher.

Evidence under `cmake-build-debug/`:

- `asset-boundaries-configure.log`
- `asset-boundaries-build.log`
- `asset-boundaries-final-build.log`
- `asset-boundaries-cook-config-build.log`
- `asset-boundaries-regression-build.log`

The initial sandboxed Xcode build could not write DerivedData; the permitted rerun completed successfully. Native Cocoa tests ran with WindowServer access.

## Automated regression results

All 11 final test processes exited 0: **1,018 passed, 3 skipped, 0 failed**.

| Test executable | Passed | Skipped |
| --- | ---: | ---: |
| FrameworkTest | 332 | 0 |
| Aurora.AdaptorTest | 107 | 0 |
| Aurora.CookTest | 64 | 0 |
| RecastNavigation.RenderTest | 6 | 0 |
| RecastNavigation.EditorTest | 6 | 0 |
| SoftBody.BuilderTest | 2 | 3 |
| EditorCoreTest | 206 | 0 |
| EditorRenderTest | 48 | 0 |
| SandboxProjectTest | 17 | 0 |
| EditorWidgetsTest | 62 | 0 |
| EditorShellTest | 168 | 0 |

Machine-readable exits are in `cmake-build-debug/asset-boundaries-tests/results.json`; each executable has a `.log` and gtest `.xml` in that directory. Framework includes 23 native Cocoa cases, all passed.

The three SoftBody skips require optional external artifacts absent from this checkout: the original seal, the original supplied cage, and the supplied benchmark matrix. They are not reported as passing.

Initial regression failures and their final fixes:

- The manifest move test expected lock ordering by manifest filename; production correctly orders by source directory, placing root before a child. The expectation now follows that contract.
- A MeshGenerator editor fixture put the default Cube only in the workspace. It now mounts an explicit temporary Engine source, matching the exact Engine-default contract.

Final logs replace the primary log names; initial diagnostic logs are retained with `.initial.log` suffixes. No production check was disabled to obtain the final pass.

Additional checks:

- CMake source ownership: 25 passed, 1 skipped, 0 failed (`asset-boundaries-source-ownership.log`).
- Python Apple bootstrap regressions: 24 passed (`asset-boundaries-python.log`).
- Ninja discovery: explicit executable path containing spaces, invalid explicit override, and PATH-only lookup passed targeted checks.
- Both new OpenSpec changes pass strict validation; `git diff --check` passes.
- Changed tracked C++ files pass the ASCII convention scan; affected code paths were reviewed for ownership, explicit failures and include visibility.

## Native editor smoke

Launched the freshly built editor through `cmake-build-debug/framegraph-opt-validation/SkyEngine FrameGraph Validation.app` and opened the existing validation project and `engine-assets-unified.world`.

1. Reopened the saved scene containing Cube and Directional Light. Cube renders and Inspector shows `Engine/shapes/cube.glb` and the configured `Engine/materials/basic_lit...` label, preserving existing asset identities.
2. Created a new Cube through Edit/Create. The new actor renders and receives the same Engine mesh and configured material; the scene contains three actors.
3. Undid the test creation, restoring the original two actors, and selected the original Cube. The editor remains open. The editor retains its dirty marker after undo; this smoke did not save over the existing scene.

Only two key screenshots were retained for this change:

- `cmake-build-debug/asset-boundaries-tests/10-reopened-scene.png`
- `cmake-build-debug/asset-boundaries-tests/11-created-cube.png`

The GLB text is a source/display label; runtime continues to load UUID-addressed AuroraMesh products. No extension rename or new runtime importer was introduced.

## Limits

- Native smoke covers scene reopen, actor creation and undo. It does not establish performance, preview-ground visual fidelity, or Vulkan/DX12 platform behavior.
- Tooltip regressions passed, but no new native hover screenshot was obtained in this run. This does not close unrelated remaining visual checks in the hover-tooltip change.
- Cross-file source mutations recover checked API failures and preserve quarantine data when compensation also fails; they are not power-loss-atomic transactions.
- ProductsOnly reads a published-product snapshot. Mounting a new overriding source alone does not rewrite that snapshot; export requires cooking/publishing the changed overlay.
- Config parsing checks UUID syntax; it does not prove a configured material exists or has the correct type. Explicit UUIDs remain dependencies whose existence/type are checked when assets/resources are loaded.
- No commit or OpenSpec archive was performed.
