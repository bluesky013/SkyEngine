## 1. Define and expose the geometry contract

- [x] 1.1 Add pure UIGeometry and affine inverse/TransformRect AABB utilities to engine/ui and UI.Interface without plugin/render dependencies.
- [x] 1.2 Add UIElement::GetGeometry(extraTransform) with on-demand ancestor composition that includes the context surface base by default and premultiplies any extra compositor transform, preserving absolute GetBounds/Arrange coordinates and current transform/pivot conventions.
- [x] 1.3 Add UIContext Set/GetSurfaceTransform with one authoritative base state used by ordinary/root Paint, default element geometry, compatibility Paint(rootTransform), and input before first paint.

## 2. Unify painting and rounded-shape coordinates

- [x] 2.1 Transform PushClip rectangles into rendered-surface AABBs and keep the root surface clip unchanged.
- [x] 2.2 Cull emitted quads against the clip after transforming their corners and computing their surface AABB.
- [x] 2.3 Evaluate rounded-shape SDFs with interpolated layout coordinates derived from shape UV/data, preserving texture-quad UV semantics.
- [x] 2.4 Share the actual Aurora UI shader source between runtime compilation and offline validation without introducing RDG/PipelinePass work.

## 3. Unify pointer routing

- [x] 3.1 Hit-test reverse stable paint/z order using inverse accumulated geometry and the same effective conservative clip; exclude non-invertible subtrees.
- [x] 3.2 Map pointer copies for every target/bubbling ancestor/hover receiver and captured pointer using current geometry, preserving absolute layout x/y and other pointer fields.
- [x] 3.3 Update direct ChildRouting to consume parent-layout coordinates, use child-local inverse and reverse paint/z order, and preserve clipping/capture consistency.
- [x] 3.4 Add coordinate-free OnPointerCancel(pointerId) for unmappable captures; clear router/direct capture before notification, forward through direct-capture containers, and cancel Button/form pressed/drag/preview state without changing focus policy.

## 4. Verify and document the boundary

- [x] 4.1 Add pure geometry regressions for affine composition/inversion/transformed AABBs and singular transforms.
- [x] 4.2 Add paint regressions for transformed clipping/culling, conservative rotated clips, unchanged surface root clip, and rounded-shape layout payload.
- [x] 4.3 Add pointer regressions for pre-paint surface setup, transformed Button clicks, per-ancestor bubbling, hover, captures/multiple pointers, singular transforms, and direct-child routes.
- [x] 4.4 Compile the shared runtime shader source offline for available supported targets, build affected UI/editor targets, and run applicable tests with unavailable platform checks recorded.
- [x] 4.5 Update docs/editor/ui-layout-and-chrome-plan.md completed states and geometry conventions without changing deferred measurement/cache/surface-ownership scope.
- [x] 4.6 Inspect contract/runtime/backend target boundaries and run strict OpenSpec validation for unify-ui-geometry.
- [x] 4.7 Add and run regressions for capture becoming singular at element/ancestor/surface level, coordinate-free target/ancestor/direct-child cancellation, Button state recovery, reflected preview/drag cleanup, and another pointer's capture remaining independent.


## Verification record

- Reconfigured the existing macOS Ninja/Release `build` directory successfully.
- Built `UICoreTest`, `UITextTest`, `UIEditorTest`, `UIShaderTest`, `EditorWidgetsTest`, `EditorShellTest`, `EditorRender`, `Aurora.Render.Editor`, and `SandboxModule`; rebuilt `Content.Views` and `SandboxEditor` so affected editor consumers use the updated element ABI. Final cancellation regressions were rebuilt after the integration build.
- Ran `ctest --test-dir build -R '^(UICoreTest|UITextTest|UIEditorTest|UIShaderTest|EditorWidgetsTest|EditorShellTest)$' --output-on-failure`: 160 cases passed (93 UI core, 8 text, 3 document editor, 6 shader, 14 editor widgets, 36 shell); 3 DXIL cases were explicitly skipped on this non-Windows host. No test failed.
- The shared runtime shader source compiled for SPIR-V and MSL at all three entries (`vs_main`, `fs_main`, `fs_round`). This is offline compiler validation, not a GPU rendering test.
- Added 15 geometry/input tests and 4 paint tests, plus the widget cancellation integration test. Coverage includes pre-paint surface transforms, retained compatibility Paint state, nested transformed Button clicks, per-ancestor bubbling, hover/capture remapping, clipping/culling, shape/texture UV semantics, direct-route z order, singular/overflow/underflow paths, independent pointer cancellation, nested direct-capture cancellation, and cancellation of a live reflected color preview without committing.
- Compiled a translation unit including only `ui/UIGeometry.h` with `-I engine/ui/include` and no plugin/render include paths. UI.Interface remains pure; UIRender continues to link UI.Interface rather than the widget runtime. The shader test links the compiler/RHI dependencies without linking UI or UIRender.
- Reconfigured `/private/tmp/sky-ui-headless-build` with its existing Sandbox=OFF settings. EditorCoreTest's generated link remains UI-free; EditorUi, EditorWidgets, EditorShellTest, and EditorWidgetsTest remain absent. This was a configuration/dependency-graph check, not a separate full headless rebuild.
- Updated the layout/chrome plan and passed strict OpenSpec validation and `git diff --check`.
- Platform limits: Windows/VS 2022, DXIL compilation, and interactive GPU/window visual verification were not run here. Existing Release assertion, duplicate-library, and third-party deployment-version warnings remain.
- No commit or OpenSpec archive was performed; the preceding two changes remain intact in the working tree.
