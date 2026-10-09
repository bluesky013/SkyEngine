> Prerequisites: `editor-asset-browser` and `asset-pipeline-layering-refactor` (both complete, pending
> archive). This change assumes the `EditorAssetCatalog` / `AssetBrowserPanel` / `AssetBuilder::DescribeSettings`
> they introduce are present. ASTC 6x6 is out of scope (block stays 4/8).

## 1. Framework: override schema and resolution

- [x] 1.1 Add `CookConfig::GetTargetSettings(assetCookJson, target) -> std::map<std::string, std::string>`
  parsing `cook.settings[target]` (flat scalars only; ignore non-scalars and non-string keys); leave
  `GetTargets` semantics unchanged.
- [x] 1.2 Add `std::map<std::string, std::string> AssetBuildRequest::settings` (`AssetCommon.h`) holding the
  resolved per-target override.
- [x] 1.3 Fill `request.settings` in `AssetBuilderManager::BuildRequest(uuid, target)` and
  `BuildRequestSync(uuid, target)` from `AssetDataBase::GetCookJson(uuid)` + `GetTargetSettings`.
- [x] 1.4 Add `AssetBuilder::{GetSettingsType, MakeSettings, DiffSettings}` (reflected settings type +
  materialize + sparse diff) and `AssetBuilderManager::{GetBuilderSettingsType, MakeBuilderSettings,
  DiffBuilderSettings}`; give `ReflectedForm`/`ReflectedFormView::Bind` an optional explicit baseline.
- [x] 1.5 Framework tests: sparse override parse; invalid/non-scalar values ignored; settings for a
  non-cooked target ignored; request plumbing carries the override.

## 2. Aurora cook: image builder applies the override

- [x] 2.1 Apply the override onto the resolved `ImageBuildConfig` in `AuroraImageBuilder::Request` for
  `encode` / `srgb` / `quality` / `block` / `maxSize` / `generateMip` (unknown keys ignored; unset keys keep
  the bundle preset).
- [x] 2.2 Declare the reflected `ImageCookSettings` type (register it + the enums in the module) and
  implement `GetSettingsType` / `MakeSettings` / `DiffSettings`.
- [x] 2.3 `AuroraCookTest`: override `maxSize`/`generateMip`; override `encode`/`quality`; unset keys fall
  back to the bundle preset.

## 3. Editor core: read and write overrides

- [x] 3.1 Add `AssetDataBase::SetCookJson(uuid, cookJson)` — replaces the manifest entry's `cook` extra
  preserving `file`/`id`, rejects read-only mounts, invalidates the manifest cache.
- [x] 3.2 Add `EditorAssetCatalog::GetCookSettings(uuid, target)` (effective reflected object + preset
  baseline + settings type) and `ApplyCookSettings(uuid, target, edited)` (diff -> sparse `cook.settings`
  write; notify observers; no auto-cook; no-op for read-only).
- [x] 3.3 (Superseded by 3.2) effective reflection now comes from the catalog's reflected settings object;
  `GetTargetInfos` retained for per-target state display.
- [x] 3.4 `EditorCoreTest`: set + reload persists; reset removes the key; read-only rejected; effective
  display reflects the override; observers notified.

## 4. Editor shell: editable settings

- [x] 4.1 In `AssetBrowserPanel` details pane, embed a `ReflectedFormView` bound to the catalog's effective
  reflected settings with the preset baseline; keep the target selector; commit edits through the catalog.
- [x] 4.2 Render read-only mounts (and assets with no settings type) non-editable (display only).
- [x] 4.3 `EditorShellTest`: selecting an asset binds the reflected form; a reflected edit persists; a
  reset-to-preset removes the override.

## 5. Verification and docs

- [x] 5.1 Verify the out-of-process worker re-derives the override from the manifest (worker loads
  manifests + `LoadBuildConfigs`); add an out-of-process integration test if feasible.
- [x] 5.2 Update `docs/editor/asset-browser.md` with the override schema (`cook.settings` per target) and
  the details-pane editing flow; call out the ASTC 6x6 non-goal.
- [x] 5.3 Run `FrameworkTest` / `AuroraCookTest` / `EditorCoreTest` / `EditorShellTest`.

## 6. ASTC 6x6 support

- [x] 6.1 Add `PixelFormat::ASTC_6x6_UNORM_BLOCK` / `ASTC_6x6_SRGB_BLOCK` (Core.h), the
  `FORMAT_INFO_TABLE` entries (Core.cpp), and the backend maps (Vulkan / DX12 / Metal) + ImageSource codes.
- [x] 6.2 `ImageBuildConfig::ResolveFormat` maps `astcBlock == 6` to the 6x6 formats (the compressor already
  takes a square block size); `Aurora.CookTest` covers the mapping.
