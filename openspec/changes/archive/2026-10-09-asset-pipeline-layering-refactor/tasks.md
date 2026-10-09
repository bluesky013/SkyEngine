## 1. Framework: mount provenance

- [x] 1.1 Add `AssetMount { id; displayName; writable; }` and expose the ordered mount list from
  `AssetDataBase` (single source: the order composed in `RebuildMounts`).
- [x] 1.2 Add the owning mount id to `AssetSourceInfo` (set at registration from `ResolveOwningFs`);
  update `assets.db` load/save and all readers. **(BREAKING)**
- [x] 1.3 Keep read-only identity rule: path-derived UUID, no manifest write; writable mounts keep
  `assets.jsonl` identity.
- [x] 1.4 Add framework tests: mount order, per-source owning mount, read-only no-write + stable id,
  writable manifest identity.

## 2. Framework: single-pass all-mount scan

- [x] 2.1 Rewrite `RebuildCacheFromScan` to iterate `GetMounts()` with one recursive walk per mount,
  testing builder extensions in the visitor.
- [x] 2.2 Add a test that a source present in both mounts resolves to the writable mount (shadowing),
  and that the scan visits each mount once.

## 3. Framework: builder self-described settings

- [x] 3.1 Add `AssetBuilder::DescribeSettings(bundle) -> vector<pair<string,string>>` (default empty).
- [x] 3.2 Add `AssetBuilderManager::GetBuilderSettings(ext, bundle)` (+ `QueryBuilder` reuse).
- [x] 3.3 Implement `DescribeSettings` in `AuroraImageBuilder` from `ImageBuildPresets`
  (encode/srgb/maxSize/generateMip/quality).
- [x] 3.4 Tests: default empty; image builder reports expected keys per bundle; unknown extension
  empty.

## 4. Cook targets and detail view

- [x] 4.1 Editor sets `activePlatform`; `EditorAssetCatalog::GetCookConfig` falls back to
  `CookConfig::GetPresetBundles(activePlatform)` when no targets are declared.
- [x] 4.2 `TriggerCook`/`BuildAllTargets` cook each resolved target (platform preset bundles).
- [x] 4.3 Detail view shows platform, targets, and per-target settings from `GetBuilderSettings`.
- [x] 4.4 Key cook state by `(uuid, bundleKey)` (from `AssetBuildResult.target`); expose an aggregate
  for the tree badge and per-target state for the detail view; show which targets have a product.
- [x] 4.5 Tests: target-less project cooks preset bundles; asset override wins; texture settings
  surface; multi-bundle cook retains each target's state and aggregates failure.

## 5. Editor core: consume provenance, single-pass tree, live state

- [x] 5.1 `EditorAssetCatalog` reads mount id/name/writable from framework provenance; delete
  `MountForPath`/`IsProjectMount`/hardcoded literals and the absolute-path computation from core.
- [x] 5.2 Add `EditorAssetCatalog::GetTree()` (single pass, deterministic); move tree assembly out of
  the panel.
- [x] 5.3 Cache structure/metadata only; query cook state live, keyed by `(uuid, bundle)` (no rebuild
  on build events); build dependents on demand.
- [x] 5.4 Make the build listener private (nested `BuildListener`); stop exposing `IAssetEvent` in the
  public header.
- [x] 5.5 Split `AssetMutationService` (Import/Move/Duplicate/Delete) and `AssetCookService`
  (`TriggerCook` + `BeginCook` marking) out of the read model (`EditorAssetCatalog`).
- [x] 5.6 Tests: mount from provenance, tree single pass/order, state live, dependents on demand,
  public header has no framework event types.

## 6. Shell: panel is a renderer

- [x] 6.1 `AssetBrowserPanel` renders the core tree; remove per-folder recursion (`AddTreeRow`).
- [x] 6.2 Fix the default bottom-dock height on the correct split (traverse to the vertical split).
- [x] 6.3 Update `EditorShellTest` for the renderer-only panel and the layout height.

## 7. Platform + build hygiene

- [x] 7.1 Add `RevealInFileExplorer(path)` to `PlatformBase` + the `Platform` facade and implement it
  in the Windows/macOS/Linux backends (mobile default no-op), alongside the other native shell
  services; call it from the module; remove `<windows.h>`/`WinExec`/`#if _WIN32` from
  `SandboxModule.cpp`.
- [x] 7.2 Pin the astcenc version via `python/third_party.py` (or assert the expected API at
  configure time) so the ABI drift cannot recur.
- [x] 7.3 Fix `configs/modules_editor.json` module names to match their DLLs.
- [x] 7.4 Aurora cleanup: cook-local enums (`ImageTypes.h`, no `aurora/rhi/Core.h` in `ImageBuildConfig`);
  `ResolveImageFormat` free function in `ImageProcess`; `AuroraImageBuilder::Request` stages split into
  `ResizeToLimit` / `GenerateMipChain` / `CompressAndEmit`.
- [x] 7.5 Keep the hashed-bundle storage layout unchanged and assume no packing; record
  `PackageAssetBundle`/`.pak` packaging as a separate future change (no implementation here).

## 8. Verification and docs

- [x] 8.1 Run `FrameworkTest` / `EditorCoreTest` / `EditorShellTest` / `AuroraCookTest`.
- [x] 8.2 Update `docs/editor/asset-browser.md` (provenance, tree, texture cook settings) and note
  the `editor-asset-browser` prerequisite.
