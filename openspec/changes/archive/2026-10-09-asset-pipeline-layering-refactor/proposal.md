## Why

A strict layering/abstraction review of the just-landed asset browser (change `editor-asset-browser`)
and the framework/Aurora pieces it touched found systemic debt: the editor core re-derives mount
identity by probing the filesystem and hardcodes "Project"/"Engine"; the asset scan hardcodes two
mounts and re-walks the tree per extension; texture cook settings live in Aurora and are unreachable;
the panel is a god-class; and the reveal-in-explorer path is platform code in the module. This change
pays that debt down so the pipeline can grow to pak/DLC mounts and richer per-kind cook config
without the editor accumulating framework knowledge.

## What Changes

- **Mount provenance (framework)**: expose the ordered mount list and the owning mount of a source
  (`id`, display name, writable) instead of the editor probing `FileExist`. **BREAKING**:
  `AssetSourceInfo` gains a mount id (or an equivalent accessor) and `AssetDataBase` exposes the
  mounts.
- **Single-pass, all-mount scan (framework)**: `RebuildCacheFromScan` iterates the mount list once,
  testing builder extensions in the visitor (no per-extension full walks). Read-only mounts keep
  path-derived identity and are never written.
- **Builder self-described cook settings (framework)**: `AssetBuilder::DescribeSettings(bundle)`
  returns ordered key/value settings (encode/srgb/maxSize/mips/…); consumers query it via
  `AssetBuilderManager`. This removes the need for the editor to know Aurora types or a preset
  filename.
- **Editor core consumes provenance (editor-core)**: `EditorAssetCatalog` reads mount id/name/writable
  from the framework (no `MountForPath`/`IsProjectMount`/hardcoded literals), builds the tree in a
  single pass, and keeps structure/metadata cached while querying cook state live (keyed by
  `uuid` + bundle, per the per-target bullet).
- **Panel becomes a renderer (editor-ui-shell)**: the tree is assembled in core; the panel only
  renders it and handles interaction (tiles, context menu, inline edit). The bottom dock's default
  height is usable, and the detail view shows the resolved targets and their effective settings.
- **Cook targets from platform presets (editor-command-registry)**: when the project declares no
  named targets, the texture cook targets are the platform's preset bundles (`common`,`tex_pc`,…), so
  Cook actually emits products; the detail view shows `platform / targets / effective settings`.
- **Per-target cook state (product storage)**: cook state is keyed by `(uuid, bundle)` so the
  multi-bundle cook of a texture (common + tex_pc) reports each product truthfully, and the detail
  view can show which product bundles exist on disk. Today state is keyed by uuid only, so the last
  bundle's result overwrites the rest.
- **Treat product storage as a framework invariant**: products are `products/<bundle>/<uuid[0:2]>/<uuid>.bin`
  with `products/<bundle>/product.index` (logical path → uuid). The refactor SHALL NOT change this
  layout; the editor only reads/subscribes. Packaging (`PackageAssetBundle`/`.pak`) is a stub and is
  explicitly out of scope (its own change).
- **Platform integration + build hygiene**: add `RevealInFileExplorer(path)` as a **native shell
  service on `PlatformBase`/`Platform`** (implemented per desktop backend, mobile no-op), like the
  existing open/save dialogs; the module no longer includes `<windows.h>`/`WinExec`. Also pin the
  astcenc version and fix `modules_editor.json` module/dll name mismatches.

## Capabilities

### New Capabilities
- `asset-mount-provenance`: ordered mount list + per-source owning mount (id/display/writable) and a
  single-pass all-mount source scan with read-only identity rules.
- `asset-builder-settings`: builders self-describe their effective per-bundle settings for display
  and cook targeting.

### Modified Capabilities
- `editor-core`: the asset catalog consumes framework mount provenance, builds the tree in one pass,
  decouples cached structure from live cook state, and tracks cook state per `(uuid, bundle)` (no
  filesystem probing in core).
- `editor-ui-shell`: the asset browser panel is a renderer over a core-built tree; the detail view
  shows the active platform, resolved targets, and per-target effective settings; the default bottom
  dock height is usable.
- `editor-command-registry`: the asset Cook action resolves targets from the platform preset bundles
  when no targets are declared.
- `native-platform-backend`: adds a native "reveal in file manager" shell service to the platform
  interface and its desktop backends.

## Impact

- Framework: `framework/asset/AssetCommon.h` (`AssetSourceInfo` mount provenance), `AssetDataBase.*`
  (mount list, scan, read-only identity), `AssetBuilder.h` + `AssetBuilderManager.*`
  (`DescribeSettings`).
- Editor core: `editor/core/asset/EditorAssetCatalog.{h,cpp}` (provenance, `GetTree`, live state),
  optional split of mutation/cook out of the read model; `EditorAssetCatalog.h` stops exposing the
  framework `IAssetEvent` in its public surface.
- Editor shell: `AssetBrowserPanel.{h,cpp}` (renderer-only), `EditorShell.cpp`.
- Editor module: `SandboxModule.cpp` (cook targets, reveal via platform service, layout ratio).
- Platform: `framework/platform/PlatformBase.h` + `Platform` facade (`RevealInFileExplorer`), and the
  Windows/macOS/Linux backends under `framework/platform/*` (mobile default no-op).
- Aurora/build: `ImageCompressor.cpp` astcenc pin/guard, `configs/modules_editor.json` names.
- Product storage: `framework/asset/AssetProductBundle.*` (`HashedAssetBundle` layout),
  `AssetManager::SaveAsset` / `product.index`, `AssetBuilderManager::SetWorkSpaceFs` (bundle dirs from
  `GetBundles()`). The `(uuid, bundle)` state change touches the editor catalog + build-event
  handling.
- Packaging: `PackageAssetBundle` is a stub (no `.pak`); out of scope here — tracked as its own
  future change.
- Related debt: `aurora-cook-schema-layering` (cook → adaptor schema dependency) — this change does
  not resolve it, but `AssetBuilder::DescribeSettings` keeps the editor out of the builder/adaptor
  layer.
- Prerequisite: `editor-asset-browser` (complete, pending archive) owns `editor-asset-catalog` /
  `editor-asset-browser`; this change refactors them and must land with/after it. Also relates to the
  active `asset-pipeline` change (deferred migration task 4.1).
- Non-goals (unchanged): grid view, drag-and-drop, rendered thumbnails, folder-creation UI, and the
  historical `(bundle,path)` UUID migration.
