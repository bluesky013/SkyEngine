# Verification

## Status and platform

Implementation, automated validation and native editor smoke completed on macOS x86_64, Xcode Debug, using the existing `cmake-build-debug` cache and third-party tree. Native acceptance resumed after the user unlocked the Mac. Windows/VS2022, Vulkan/DX12 and Linux were not built or run in this session.

The target/metadata/editor-policy implementation is owned by this change. Earlier asset-reference fixes, project creation defaults and hover hints remain in their respective active changes. No commit or archive has been performed.

Final OpenSpec strict validation and `git diff --check` pass. The 154 changed or untracked C++ headers/sources checked contain only ASCII. All implementation and acceptance tasks are complete; the active change remains ready for combined code/spec review.

## Build and implementation boundaries

Final real-library/module/tool/editor build succeeded, including Aurora.Assets.Static, AuroraCook.Static, Aurora.Cook, Aurora.Adaptor.Static, Aurora.Metadata, Aurora.Render, Aurora.Render.Editor, SandboxModule, SandboxEditor, AssetTool, SoftBody.Builder and RecastNavigation.Editor. The affected test executables were rebuilt. A final fixture build also covered EditorRenderTest, EditorShellTest and Animation.ModuleTest after correcting their explicit runtime metadata dependencies.

Evidence in `cmake-build-debug/`:

- `architecture-boundaries-final-build.log`
- `architecture-boundaries-fixtures-configure.log`
- `architecture-boundaries-fixtures-build.log`
- `architecture-boundaries-source-ownership.log`

The ownership audit passes 25 cases and skips the Windows-specific path-identity case on macOS. Real CMake configuration also completes its production source-owner check.

`architecture-boundaries-tests/target-boundaries.json` records actual artifact inspection:

- Assets contains six production objects: MaterialDefinition, MaterialReflection, MeshAsset, SkinAsset, AuroraAssetReflection and LodGroupAsset.
- Full symbol scans of Assets, Metadata and Cook contain no SceneAssetSource, RendererHost, runtime component registration, eight concrete runtime component implementations, or the six checked runtime conversion entrypoints.
- Assets and Metadata contain the persistent registration implementation. Cook tests no longer include the renderer conversion interface.
- All 28 original named reflection registrations remain represented exactly once across the two owners; seven asset handlers belong to Assets and eight component persistent mappings match the runtime component identities.

This does not claim complete renderer-free or GPU-framework-free linkage: the approved existing Aurora.Core.Static dependency remains, and shared platform dependencies include Metal.framework.

## Automated tests

Final result: **1,062 passed, 3 skipped, 0 failed**, counting unique test cases per executable. The six shader-product cases initially skipped without a package path were subsequently run successfully in isolated shipping-style hosts and count as passed, not both skipped and passed.

| Executable | Passed | Skipped |
| --- | ---: | ---: |
| FrameworkTest | 332 | 0 |
| Aurora.AdaptorTest | 108 | 0 |
| Aurora.CookTest | 66 | 0 |
| RecastNavigation.RenderTest | 6 | 0 |
| RecastNavigation.EditorTest | 6 | 0 |
| SoftBody.BuilderTest | 2 | 3 |
| EditorCoreTest | 208 | 0 |
| EditorRenderTest | 48 | 0 |
| SandboxProjectTest | 22 | 0 |
| EditorWidgetsTest | 62 | 0 |
| EditorShellTest | 168 | 0 |
| Aurora.Renderer.ModuleTest | 16 | 0 |
| Aurora.Renderer.StaticModuleTest | 16 | 0 |
| Animation.ModuleTest | 2 | 0 |

The three SoftBody skips require optional external seal/cage/benchmark fixtures that are absent. Framework's native Cocoa cases ran with WindowServer access. The Windows-only GpuSubmissionTest metadata main was migrated in source but could not be compiled on this platform.

Logs, gtest XML, process exits and the deduplicated summary are under `cmake-build-debug/architecture-boundaries-tests/` (`results.json`, `products-results.json`, `summary.json`). Initial failing EditorRenderTest/EditorShellTest logs and process results are retained separately with `.initial` names. Those fixtures originally loaded only Aurora.Metadata while using runtime components; their gtest Environments now explicitly load the runtime owner through ModuleManager. No reflection guard or repeated SetUp registration was added.

New or preserved regression coverage includes persistent-only schemas for all eight Aurora components, World binary round trips and roots, failed Init/retry/manager recreation, Cook preset retention across module sessions, domain handler conflicts and errors, wrong animation skeleton field types, failed attachment/execution compensation, undo/redo and document/actor lifetime checks. Cook image payload tests and Adaptor conversion tests now exercise their own compilation boundaries.

## Real metadata-only World cook

Ran `cmake-build-debug/asset-editor-boundaries-validation/verify_metadata_cook.py` against the freshly built AssetTool. The isolated engine has no source assets and loads only Aurora.Metadata, Aurora.Cook, Content.Metadata and Content.Builder.Module. Aurora.Render is not loaded.

The tool produced one 1,225-byte World product with zero cook failures. Its envelope checksum, UUID, two nonzero mesh/material dependencies, World roots, two actors (Cube and Directional Light), four component identities and persisted data identities were checked against the authored document. This proves World compilation and reference preservation, not that the referenced mesh/material products were included in this small fixture.

Evidence: `architecture-boundaries-metadata-cook.log` and `asset-editor-boundaries-validation/metadata-only/result.json`, both under `cmake-build-debug/`. The sandboxed tool emitted a macOS LaunchServices notification diagnostic, but exited 0 and passed all product checks.

## Products-only runtime

Reused the existing version-10 Metal shader product package with current binaries. The repository's `copy_consumer` helper copied each latest dynamic/static test executable and required runtime modules into separate directories without shader sources or compiler libraries. `run_consumer` cleared loader-path overrides, enabled loaded-library diagnostics and rejected skipped/zero tests or a loaded Slang compiler.

Both hosts passed all three `RendererModule.ShippedProducts*WithoutCompiler` cases: scene rendering, explicit compute/fullscreen passes, and named material/image-version resources. These runs used the actual Intel Iris Plus Graphics 655 Metal device. No old stripped-host executable was reused.

Evidence: `architecture-boundaries-products-only.log`, the two `.products.xml` files, and `architecture-boundaries-tests/products-results.json`.

## Native editor acceptance

Launched the validation app with the freshly built SandboxEditor and current modules, opened the existing Test project, and reopened `engine-assets-unified.world`. Cube and Directional Light appeared in the Outliner, the Cube rendered, and selecting it showed the reflected Transform/StaticMesh fields. The mesh displayed `Engine/shapes/cube.glb`; the material displayed the readable `Engine/materials/basic_lit...` label.

The Edit/Create menu exposed Empty Object, Cube, Plane, Point Light, Directional Light, Camera and Environment. Creating Camera added and selected a third actor, with Transform and CameraComponent fields visible. Toolbar Undo removed it and returned the Outliner to the original two actors. The editor remains open with Cube selected; the authored World was not saved over. Its dirty marker remains after undo, consistent with the existing history behavior.

Three key screenshots are retained under `cmake-build-debug/architecture-boundaries-tests/`:

- `20-reopened-scene.png`: rendered scene and readable asset labels.
- `21-created-camera.png`: migrated creation action and reflected Camera fields.
- `22-after-undo.png`: restored actor list and original Cube selection.

This smoke did not repeat the earlier manual hover-tooltip acceptance or exercise skeletal editing through native UI; those paths are covered by their existing/new automated regressions. No visual or platform coverage beyond the steps above is inferred from test success.
