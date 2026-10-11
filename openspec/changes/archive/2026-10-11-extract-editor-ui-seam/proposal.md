## Why

Feature editor-extension modules (`engine/content/editor` -> `Content.Views`, `engine/aurora/editor` ->
`Aurora.Render.Editor`) supply custom asset views. They should depend only on the UI-free editor contract
(`EditorCore`) or on UI primitives — never on the whole editor shell. Today the UI-facing extension seam
(`IAssetViewProvider` + `AssetViewProviderRegistry`) and the draw primitives (`UiDraw`, `UiTheme`) live inside
`EditorShell`, so any extension that returns a `UIElement` is forced to link the entire shell. Because those
extension modules were also configured in the engine base phase (before the sandbox editor framework exists),
builds broke in sandbox-OFF configs (they referenced a framework that is not built) and were mis-ordered in
sandbox-ON configs.

## What Changes

- Extract a new UI-dependent editor module **`EditorUi`** (`engine/sandbox/ui/`, target `EditorUi`) containing
  the UI primitives (`UiDraw`, `UiTheme`) and the asset-view provider seam (`IAssetViewProvider` +
  `AssetViewProviderRegistry`), depending only on `Core` + `UI`.
- Move the viewer-policy helper `AssetViewFlags` + `DeriveAssetViewFlags` out of the seam into `EditorShell`
  (it composes `Framework` (`AssetBuilderManager`) + `EditorCore` (`AssetViewDataRegistry`)).
- Repoint `Aurora.Render.Editor` (and keep `Content.Views`) to depend on `EditorUi` / `EditorCore` — **not**
  `EditorShell`; includes move from `editor/shell/**` to `editor/ui/**`.
- Build the feature editor-extension modules in the **editor phase** (after `add_subdirectory(sandbox)`), gated
  by `SKY_BUILD_SANDBOX`.
- **BREAKING** (build/layout): `engine/aurora/editor` no longer links `EditorShell`;
  `editor/shell/UiDraw.h`, `editor/shell/UiTheme.h`, `editor/shell/AssetViewProvider.h` are removed/moved.

## Capabilities

### New Capabilities

- `editor-extension-ui`: the `EditorUi` module (UI primitives + UI-facing extension seam), its dependency
  position between `EditorCore` and `EditorShell`, and the rule that feature editor extensions link it rather
  than the shell.

### Modified Capabilities

- `editor-asset-viewer`: the `AssetViewProviderRegistry` / `IAssetViewProvider` seam is now exposed by the
  `EditorUi` module (not the shell), and `AssetViewFlags` derivation is shell-owned viewer policy.

## Impact

- `engine/sandbox/ui` (new `EditorUi` module), `engine/sandbox/shell` (depends on `EditorUi`; owns
  `AssetViewFlags`), `engine/aurora/editor`, `engine/content/editor` (link `EditorUi`; includes `editor/ui/*`).
- `engine/CMakeLists.txt`, `engine/{content,aurora}/CMakeLists.txt`, `engine/sandbox/CMakeLists.txt`,
  `engine/sandbox/shell/CMakeLists.txt`.
- Docs: `docs/editor/{asset-browser,ui-layout-and-chrome-plan,reflection-widget-framework}.md`,
  `engine/aurora/AGENTS.md`.
- No runtime behavior change; asset-view rendering/editing is unchanged.
