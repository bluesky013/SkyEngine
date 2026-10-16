## 1. Establish UI foundation target boundaries

- [x] 1.1 Restrict the root `UI.Interface` target to provider contracts, referenced data, and pure utilities, keeping `ui/...` include paths stable; the surrounding `engine/ui` foundation may contain implementations.
- [x] 1.2 Keep retained widgets, text fallback, document/runtime data, and input implementation in `engine/ui/core` as `UI`; remove editor, Aurora, Framework, and FreeType implementation dependencies from core.
- [x] 1.3 Keep the Aurora renderer separately in `engine/ui/render`, preserving `UIRender` and the existing draw-data contract.
- [x] 1.4 Keep the optional font provider in `engine/ui/freetype` as `UI.FreeType`, and explicitly link it from provider callers only when enabled.
- [x] 1.5 Keep headless document editing in `engine/ui/editor` as `UI.Editor`; split `UIEditorTest` so runtime UI tests do not link document-editing implementation.
- [x] 1.6 Configure these implementation subtargets from the engine UI foundation root without a UI plugin entry; preserve runtime `UI` and `UIRender` consumer compatibility.

## 2. Complete arrangement before paint

- [x] 2.1 Add `UIElement::Arrange(finalRect)` and route existing anchor/size `Layout(parentRect)` through it without changing the unconstrained Measure API.
- [x] 2.2 Move StackPanel, GridPanel, DockPanel, and Overlay allocation into recursive arrangement hooks; arrange nested children through the common entry.
- [x] 2.3 Remove layout mutations from the migrated slot containers' paint callbacks, keeping convenience layout entry points on the same arrangement path.
- [x] 2.4 Migrate affected Sandbox and material-editor compositions to arrange before child-bound reads, input routing, and painting.

## 3. Verify the architecture and regressions

- [x] 3.1 Add focused regression tests for geometry before first paint, nested slot containers, resize/rearrangement, and paint preserving arranged child bounds.
- [x] 3.2 Reconfigure and build runtime UI/text/document-editor targets; run `UICoreTest`, `UITextTest`, and `UIEditorTest`.
- [x] 3.3 Verify FreeType enabled/disabled dependency separation and run `UITextFreeTypeTest` where the font dependency is available.
- [x] 3.4 Build affected Sandbox, EditorRender, and material-editor targets; run applicable editor tests and record any platform validation that remains unavailable.
- [x] 3.5 Inspect the final dependency/header boundaries and run `openspec validate refactor-ui-foundations --strict --no-interactive`.

## Verification record

Placement revision (2026-10-17): the current task paths above follow the user's explicit UI foundation exception. The execution record below belongs to the earlier target split, when implementations used the plugin layout. Its commands/counts are retained as historical evidence, not reruns against the current placement. The 75-file return to `engine/ui` and latest review are recorded separately in `simplify-ui-foundation-boundaries` and canonical §19.

- Reconfigured the existing `cmake-build-debug` Xcode project successfully. Xcode compilation was prevented by sandbox access to its DerivedData cache; no source diagnostic was reported by that attempt.
- Used the existing macOS `build` Ninja/Release configuration with the same `build_3rd/MacOS-x86` dependencies. Built `UICoreTest`, `UITextTest`, `UIEditorTest`, `UITextFreeTypeTest`, `EditorCoreTest`, `EditorShellTest`, `EditorRender`, `Aurora.Render.Editor`, and `SandboxModule`.
- Passed 227 tests: UICoreTest 74, UITextTest 8, UIEditorTest 3, UITextFreeTypeTest 2, EditorCoreTest 104, EditorShellTest 36. This includes nine new layout regressions.
- Configured a separate headless build with Sandbox and FreeType disabled. Confirmed the optional font/shell targets are absent and UI compile commands have no Framework, Aurora, Sandbox, or FreeType include/definition dependencies. This check covers configuration and dependency separation; the complete headless configuration was not built.
- Compiled and ran a standalone provider/layout-data consumer with only `engine/ui/include` and `UI.Interface`; no widget/plugin/Core library was needed.
- Audited all 78 original `engine/ui` files: every non-CMake file has a destination in the new layout. `git diff --check` and strict OpenSpec validation passed.
- Windows/VS 2022 compilation and interactive GPU/window/DPI verification remain platform follow-ups; they were not run on this macOS host. No commit or OpenSpec archive was performed.
