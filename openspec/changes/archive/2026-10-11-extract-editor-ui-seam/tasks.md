## 1. Extract the `EditorUi` module

- [x] 1.1 Create `engine/sandbox/ui` -> target `EditorUi` (STATIC, deps `Core` + `UI`)
- [x] 1.2 Move `UiDraw`, `UiTheme`, and `AssetViewProvider` (`IAssetViewProvider` + `AssetViewProviderRegistry`) to `editor/ui/*`
- [x] 1.3 Remove `AssetViewFlags` / `DeriveAssetViewFlags` from the seam header

## 2. Shell owns viewer policy

- [x] 2.1 Add `editor/shell/AssetViewFlags.h` + `src/AssetViewFlags.cpp` (moved `DeriveAssetViewFlags`)
- [x] 2.2 Update shell consumers (`AssetViewerWidget`, `EditorShell`) to include it
- [x] 2.3 `EditorShell` links `EditorUi`

## 3. Repoint the editor extensions

- [x] 3.1 `aurora/editor` links `Aurora.Adaptor.Static + EditorCore + EditorUi + UI` (not `EditorShell`)
- [x] 3.2 `content/editor` links `Content.Adaptor + EditorCore` (no shell)
- [x] 3.3 Rewrite include sites (`editor/shell/{UiDraw,UiTheme,AssetViewProvider}.h` -> `editor/ui/*`)
- [x] 3.4 Add `editor/ui/...` includes at the flag-derivation use sites

## 4. Build phase

- [x] 4.1 `engine/CMakeLists.txt`: configure `content/editor` + `aurora/editor` after `sandbox`, gated by `SKY_BUILD_SANDBOX`
- [x] 4.2 `engine/sandbox/CMakeLists.txt`: add `ui` before `shell`
- [x] 4.3 Remove the unconditional `add_subdirectory(editor)` from `engine/content` and `engine/aurora`

## 5. Documentation

- [x] 5.1 `engine/aurora/AGENTS.md`: `Aurora.Render.Editor` deps (`EditorCore`/`EditorUi`, no `EditorShell`)
- [x] 5.2 `docs/editor/asset-browser.md`: provider path `editor/ui` + `EditorUi` file entry
- [x] 5.3 `docs/editor/ui-layout-and-chrome-plan.md` + `reflection-widget-framework.md`: module placement

## 6. Verification

- [x] 6.1 sandbox-OFF (`cmake-build-debug`) full build succeeds
- [x] 6.2 sandbox-ON (`build`) editor targets (`EditorUi`, `EditorShell`, `EditorRender`, `SandboxModule`, `Content.Views`, `Aurora.Render.Editor`, `SandboxEditor`) succeed
- [x] 6.3 Confirm `EditorUi` does not link `EditorCore` / `EditorShell` (generated project refs)
