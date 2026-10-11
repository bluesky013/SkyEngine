## Context

The non-Qt editor framework is layered: `EditorCore` (UI-free: services, layout model, extension contract) ->
`EditorShell` (composes `sky::ui`) -> `EditorRender` / `SandboxModule`. Feature editor-extension modules
(`engine/content/editor` -> `Content.Views`, `engine/aurora/editor` -> `Aurora.Render.Editor`) register
per-asset views and are loaded at runtime by the editor. Some of them supply a **custom `UIElement` widget**
(notably the aurora material editor).

Two defects:
1. The UI-facing seam (`editor/shell/AssetViewProvider.h`: `IAssetViewProvider` + `AssetViewProviderRegistry`)
   and the draw primitives (`editor/shell/UiDraw.h`, `editor/shell/UiTheme.h`) lived in `EditorShell`, so any
   extension returning a `UIElement` had to link the whole shell. `UIElement` itself lives in `UI`
   (`engine/ui`) — the shell link was an artifact of header placement, not a real dependency.
2. Those extension modules were configured in the engine base phase (before `add_subdirectory(sandbox)`), so in
   sandbox-OFF configs they referenced a framework (`EditorCore`/`EditorShell`) that is never built, and in
   sandbox-ON configs they forward-referenced later-defined targets.

## Goals / Non-Goals

**Goals:**

- Extract a UI-dependent editor module `EditorUi` holding the UI primitives + UI-facing extension seam, so
  feature editor extensions depend on `EditorCore`/`EditorUi` and never on `EditorShell`.
- Keep `EditorCore` UI-free (its header guard stays satisfied).
- Keep the material editor's custom widget (do not degrade it to a reflected form).
- Build the extension modules in the editor phase, gated by the editor build option.

**Non-Goals:**

- Changing asset-view runtime behavior (rendering, editing, cook).
- Removing custom providers or unifying the two seams (`IAssetViewDataProvider` vs `IAssetViewProvider`).
- Moving `EditorCore` or changing the `UI` toolkit.

## Decisions

### Decision: New `EditorUi` module (Core + UI)

Create `engine/sandbox/ui` -> target `EditorUi` (STATIC) containing `editor/ui/UiDraw.{h,cpp}`,
`editor/ui/UiTheme.{h,cpp}`, and `editor/ui/AssetViewProvider.h` (seam: `IAssetViewProvider` +
`AssetViewProviderRegistry`). Deps: `Core` + `UI` only.

Alternatives: putting the seam in `EditorCore` (rejected — `EditorCore` is guarded UI-free and the seam returns
`sky::ui::UIElement`); keeping everything in `EditorShell` (rejected — forces extensions to link the shell).

### Decision: Move viewer policy to the shell

`AssetViewFlags` + `DeriveAssetViewFlags` moved to `EditorShell`
(`editor/shell/AssetViewFlags.{h,cpp}`). It composes `Framework` (`AssetBuilderManager`) +
`EditorCore` (`AssetViewDataRegistry`) + the seam registry — viewer policy, not a plugin-facing contract. This
keeps `EditorUi` free of `Framework`/`EditorCore` deps.

### Decision: Dependency direction

`EditorCore (UI-free) <- EditorUi (Core+UI) <- EditorShell`; feature editor extensions depend on
`EditorCore`/`EditorUi`. `EditorUi` MUST NOT depend on `EditorShell` or `EditorCore`.

### Decision: Build phase for editor extensions

`engine/CMakeLists.txt` adds `content/editor` and `aurora/editor` **after** `add_subdirectory(sandbox)`, inside
`if (SKY_BUILD_SANDBOX)`. They depend on the editor framework, so they are correctly gated and ordered.

### Decision: Include paths

Moved headers are included as `editor/ui/*`; `UiSkin` (component skin) stays in `EditorShell`. ~40 include
sites in the shell were updated mechanically.

## Risks / Trade-offs

- [Include-path churn across ~40 shell files] -> Mechanical, verified by build in both configs.
- [`engine/sandbox/ui` sits near `engine/ui`] -> Distinct `editor/ui/` include prefix keeps them unambiguous.
- [Extensions that render UI keep a `UI` dependency] -> Accepted: this change keeps the custom-widget path
  (Option 1′) rather than degrading to registration-only.

## Migration Plan

Applied in place (build/layout only, no runtime migration). Verified: sandbox-OFF full build and sandbox-ON
editor-target build both succeed.

## Open Questions

- None blocking.
