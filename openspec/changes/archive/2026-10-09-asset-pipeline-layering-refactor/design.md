## Context

`editor-asset-browser` (complete, pending archive) landed the editor-side asset browser on top of the
`asset-pipeline` framework APIs. A strict review found the editor core knows too much about the
framework's mounts and filesystems, the panel mixes model and view, texture cook settings are
unreachable, and a few framework/Aurora rough edges. This change is a layering/abstraction pass; it
does not add user-facing features beyond a usable texture cook display and a working Cook action.

Current facts (verified):
- `AssetDataBase` builds an ordered `MultiFileSystem` in `RebuildMounts()` but exposes no mount list;
  `AssetSourceInfo` has no mount field. `RebuildCacheFromScan` hardcodes `workSpaceFs`/`engineFs`.
- `EditorAssetCatalog` rediscovers mount identity by `GetWorkSpaceFs()->FileExist()` and literals
  `"Project"`/`"Engine"`; `GetAbsolutePath` concatenates FS roots in core.
- `CookConfig::GetTargets` returns asset overrides else project `targets`; `asset_build_presets.json`
  has no `targets`, so `BuildAllTargets` is a no-op. Per-bundle texture settings live only in
  Aurora's `ImageBuildPresets` (`image_build_presets.json`).
- `AuroraImageBuilder` resolves `request.target` (a bundle) via its presets; the editor cannot see
  those settings without depending on Aurora.

## Goals / Non-Goals

**Goals**
- Framework owns mount identity/provenance; editor consumes it.
- One-pass, all-mount source scan; read-only mounts never written.
- Builders self-describe effective per-bundle settings; editor shows them generically.
- Editor core holds no UI/FS-probing logic; the panel is a renderer.
- A working texture cook target list and a usable detail view.

**Non-Goals**
- Grid view, drag-and-drop, drop-file import, rendered thumbnails, folder-creation UI.
- Historical `(bundle,path)` UUID migration (asset-pipeline task 4.1).
- Adding real pak/DLC mounts (only the abstraction to accept them).

## Decisions

### D1. Mount provenance is a framework concept
Add `struct AssetMount { std::string id; std::string displayName; bool writable; }` and expose the
ordered mount list from `AssetDataBase` (single source: the same order `RebuildMounts` composes).
`AssetSourceInfo` carries the owning `mount` id, set at `RegisterAsset` from the resolved
`ResolveOwningFs`. The editor maps `id → displayName` for the tree roots and reads `writable` from the
mount, deleting `MountForPath`/`IsProjectMount`/`kProjectMount`/`kEngineMount` and `GetAbsolutePath`'s
roots probing. **BREAKING**: `AssetSourceInfo` layout changes (rebuild consumers).

### D2. One-pass, all-mount scan
`RebuildCacheFromScan` iterates `GetMounts()` and performs a **single** recursive walk per mount,
testing membership in the builder-extension set in the visitor (replacing per-extension walks).
Read-only mounts derive stable identity via `CalculateUuidByPath` and never write manifests; writable
mounts read/write `assets.jsonl` as today.

### D3. Builder self-described settings
Add an optional `virtual std::vector<std::pair<std::string,std::string>>
AssetBuilder::DescribeSettings(const ProductBundleKey &bundle) const { return {}; }`. Add a manager
helper `AssetBuilderManager::GetBuilderSettings(const std::string &ext, const ProductBundleKey &)`.
`AuroraImageBuilder` implements it from `ImageBuildPresets`. The editor queries it via the asset's
extension, so core never names a builder kind or a preset file.

### D4. Cook-target semantics
When an asset has no `cook.targets` override and the project declares no named targets, the effective
cook targets are the platform's preset bundles (`CookConfig::GetPresetBundles(activePlatform)`;
e.g. `{common, tex_pc}`). The editor sets `activePlatform` (host platform) so this resolves. Cook and
the detail view use the same list; the detail view pairs each target with `DescribeSettings`.

### D5. Editor core consumes provenance; core has no FS probing
`EditorAssetCatalog`:
- reads `mount`/`displayName`/`writable` from the framework (D1); items carry the mount id;
- exposes a single-pass tree (`GetTree()` returning the root `EditorAssetFolder`); the public API no
  longer exposes any filesystem/absolute-path logic;
- caches structure/metadata only; cook state (`state`/`error`) is queried live, **keyed by
  `(uuid, bundle)`**, so build events do not invalidate the structure cache (see D9);
- keeps the `IAssetEvent` listener private (nested `BuildListener`) so the public header does not
  expose framework event types.
- `GetAbsolutePath` moves to the platform service (D7) or a provenance helper.

### D6. Panel is a renderer
`AssetBrowserPanel` renders the core `EditorAssetTree`, does tile layout + hit-testing, context menu,
and inline edit only. Tree assembly and folder grouping move to core (D5). The default layout gives
the bottom dock a usable height by setting the correct split ratio (found by traversal, not by
guessing a node).

### D7. Platform integration (cross-platform native shell service)
Add `virtual void RevealInFileExplorer(const std::string &path)` to `PlatformBase` (default no-op)
plus the `Platform` facade method, mirroring the existing `ShowOpenFileDialog`/`ShowSaveFileDialog`
native shell services. Desktop backends (`framework/platform/windows|macos|linux`) implement it
(Windows: `explorer /select`); mobile/other backends keep the default no-op. The editor module calls
`Platform::Get()->RevealInFileExplorer(absPath)` and drops `<windows.h>`/`WinExec`/`#if _WIN32`. The
absolute path is obtained through framework provenance (D1), not constructed in core.

### D8. Build hygiene
Pin the astcenc version via `python/third_party.py` (or assert the expected API at configure time) so
the `astcenc_context_alloc` drift cannot silently break `Aurora.Cook` again. Fix
`configs/modules_editor.json` module names to match their DLLs.

### D9. Product storage is a framework invariant; per-target state

Products are stored per bundle as `products/<bundle>/<uuid[0:2]>/<uuid>.bin` with a
`products/<bundle>/product.index` keyed by canonical logical path → uuid (a `HashedAssetBundle`).
This refactor SHALL NOT change that layout; the editor only reads/subscribes and, for the detail view,
reports which bundles have a product. Because a texture cooks into several bundles
(`common` + `tex_pc`), editor cook state SHALL be keyed by `(uuid, bundleKey)` — `AssetBuildResult`
already carries `target`, so the catalog keys states by that pair and exposes the worst/aggregate for
a uuid (used by the tree badge) plus per-target entries (used by the detail view). A single-key state
today loses all but the last bundle. Product presence is queried through the framework
(`AssetManager`/bundle lookup), never by the editor constructing storage paths.

### D10. Packaging is out of scope

`PackageAssetBundle` is a stub (`IsPacked()=true`, `OpenFile`/`CreateOrOpenFile` return null); there
is no `.pak` writer. This change does not implement packaging. The relevant constraint is only that
the refactor must not assume packing exists and must keep the hashed-bundle layout addressable, so a
future packaging change can wrap it (mount a package FS, emit `product.index` into the package).

## Risks / Trade-offs

- **`AssetSourceInfo` is public framework surface** → update all writers/consumers in one step; keep
  `assets.db` load/save and read-only identity semantics intact; run `FrameworkTest` asset suites.
- **Scanning engine registers many sources** → memory grows; acceptable for now, note lazy/per-kind
  loading as future work.
- **`DescribeSettings` interface addition** → default implementation keeps existing builders valid.
- **Cook-target semantics change** → projects that already declare named targets keep them; only
  target-less projects gain the platform-bundle fallback.
- **`editor-asset-browser` prerequisite** → this change must land together with or after it, since it
  edits the same catalog/panel files.

## Migration Plan

1. D1 + D2 (framework provenance + scan) with framework tests (mount list, read-only identity,
   single-pass order).
2. D3 + D4 (builder settings + cook targets) with a texture-preset display + cook test.
3. D5 + D6 (core provenance/tree/state, panel renderer) with editor tests.
4. D7 + D8 (platform service, astcenc pin, module names).

Rollback: each step is independent; reverting D3/D4 restores the previous (no-op) cook without
touching D1/D2.
