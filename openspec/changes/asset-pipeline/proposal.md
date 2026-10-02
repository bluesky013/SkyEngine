## Why

Source-asset identity is currently **derived from the file path**: `AssetDataBase::RegisterAsset` computes `Uuid::CreateWithSeed(hash(bundle, path))` every time (`engine/framework/src/asset/AssetDataBase.cpp:17-23,39-42,122`). Because serialized references (components, material/mesh/lod payloads) are already `Uuid`, any rename or move changes the identity and silently breaks every reference. The only mapping table (`assets.db`) is rewritten wholesale from an unordered map (`AssetDataBase.cpp:259-308`), so it is unusable as a committed, mergeable source of truth across branches, and it is currently also relied on at runtime for path→uuid lookup.

Alongside identity, the loading layer is not cleanly separated from authoring: some product payloads still store **source paths** (legacy `MaterialAsset` JSON, `MaterialAsset.cpp:173-175,227-229`) and resolve them against the source catalog at load time (`:192-193`), and the asset async executor needlessly depends on taskflow even though the engine already ships `sky::ThreadPool`.

The engine needs stable merge-friendly source identity, a product-only loading path shared by editor and runtime, and an asset executor built on the engine's own thread pool.

## What Changes

- **BREAKING**: source-asset UUID stops being a pure function of the path. It is assigned once and persisted in a **per-directory manifest** (`assets.jsonl`, JSON Lines) alongside the source files, then frozen for the life of the asset.
- Manifests follow **D1**: one text manifest per directory, one sorted line per source file (`filename<TAB>uuid`), committed to VCS. Identity changes are localized to a single directory, so multi-branch merges only conflict on the directory that actually changed.
- The **engine asset tree uses the same mechanism** as the workspace (per-directory manifests committed with the repo); no central identity table.
- `assets.db` stops being the identity authority and becomes a **dev/build-side cache only**; it is neither shipped nor read at runtime. Runtime path→uuid lookup uses a **generated product index** emitted into the product bundle, and source `assets.jsonl` manifests are authoring/build-side only.
- **Migration preserves existing references**: migration scans the source trees (by builder-known extensions) and seeds each manifest entry with the **legacy** path-derived UUID (`CalculateUuidByPath(currentPath)`); `assets.db`, when present, is an optional cross-check. Only brand-new assets get a fresh identity; moved/renamed assets keep theirs.
- **Asset type identity**: the persisted source `category` is replaced by a single **`AssetTypeId`** — derived for sources from the builder registry (extension) and carried in the product header for products. Type-based selection is a consumer concern; editor wiring is deferred to the sandbox editor refactor.
- **Unified loading layer**: the editor and the runtime use the **same** product-based loader. Path→uuid resolves through the generated product index (D4), with an **editor-only source-catalog fallback** for not-yet-cooked assets (D3). The editor loads **only cooked products** (never source formats), and **product payloads reference assets by UUID only** (no source paths).
- **On-demand cook with a completion event**: in the editor, loading an asset whose product is missing (but whose source exists) **triggers a cook** and returns the asset in a **LOADING** state, which becomes LOADED on cook success or **FAILED** on cook failure (never a source fallback); the runtime (no source catalog, no cook) treats a missing product as a hard error. Cook runs either **in-process** or in a **separate process**, selectable by config, and both raise the same completion event.
- **Source-asset move / rename / delete / duplicate** semantics (UUID preserved on move/rename, reassigned on duplicate) update manifests while leaving all references untouched; editor UI wiring is deferred to the sandbox editor refactor.
- **Asset async execution migrates off taskflow** onto the engine's existing `sky::ThreadPool` (`engine/core/include/core/async/ThreadPool.h`); `Framework`'s own asset code and public headers (`Asset.h`, `AssetExecutor.h`) stop using/exposing taskflow. (Framework's *transitive* build dependency, which flows through `Core`, is deferred — see below.)
- World documents obtain identity through the resolver instead of recomputing `CalculateUuidByPath` (`engine/editor/src/document/Document.cpp:87-93`); the editor document wiring lands with the sandbox editor refactor.
- **Source import / delete flow**: importing a source copies it into the workspace, assigns identity, and optionally cooks the current platform; deleting removes the file and its identity.
- **Cook configuration (project + asset)**: a project-level cook config defines platform→target presets (encoder, mips, compression) per asset kind; each source asset can override them. One source asset may emit products for multiple platform targets (same UUID, per-bundle products).
- **Cook data compression**: cook output may be compressed; it reuses the existing framework `CompressionManager` / `ICompressor`, with the `CompressionModule` registering lz4; the product header records the codec and the loader decompresses.
- **Text storage**: `assets.jsonl` (JSON Lines), `configs/asset_cook.jsonc`, and `product.index` are text (UTF-8, LF, canonical `/` paths) and directly editable.
- **Mount-based logical namespace**: source roots become ordered mounts in the engine `MultiFileSystem` (`core/file/MultiFileSystem.h`) — workspace writable first, engine read-only next, with custom/pak/DLC adding more. Asset keys become a single logical path, and `SourceAssetBundle`/`sourceBundle` are removed; precedence is the mount order.
- **Asset dependency graph**: an `IAssetDependencyProvider` exposes forward `Dependencies` and reverse `Dependents` (computed by inversion), assembled from product headers at runtime and the source dependency index in the editor; dependencies are not stored in `assets.jsonl` or `product.index`.

## Capabilities

### New Capabilities

- `asset-source-identity`: per-directory manifest (`assets.jsonl`) format/location, one-time UUID assignment, migration seeding, path/uuid resolution, a mount-based logical namespace (`MultiFileSystem`), stable identity for generated sources (world documents), a single `AssetTypeId` (source-derived, product-header-backed), and the asset dependency graph (`IAssetDependencyProvider`).
- `asset-source-mutation`: source-asset move/rename/delete/duplicate that preserves or reassigns UUID and updates manifests without touching references (framework APIs; editor UI wiring deferred).
- `asset-product-loading`: a single product-based loading path shared by editor and runtime, path→uuid via the generated product index, UUID-only product payloads, and on-demand cook with a completion event (in-process or out-of-process) in the editor.
- `asset-cook-config`: source import/delete flow, project-level + asset-level cook configuration, multi-platform target outputs, and cook-data compression.
- `asset-async-executor`: asset loading/saving async uses `sky::ThreadPool` with dependency-ordered execution, and no taskflow type is exposed by `Framework` public headers.

### Modified Capabilities

- (none)

## Impact

- `engine/framework/src/asset/AssetDataBase.cpp` + `include/framework/asset/AssetDataBase.h`: identity assignment/resolution, manifest IO, and the editor-backed source catalog (`ResolvePath`/`Exists`/`GetTarget`); drop `CalculateUuidByPath` as the identity source.
- `engine/framework/include/framework/asset/AssetCommon.h` (`AssetSourcePath`, `SourceAssetBundle`) and `AssetDataBase.*`: replace the bundle enum with a logical path over the `MultiFileSystem` mount namespace (workspace writable, engine read-only).
- `engine/framework/src/asset/AssetManager.cpp` + `AssetProductBundle.*`: own the product path→uuid map loaded from each bundle's generated product index (kept out of `assets.db`); it is **level 1** of path resolution, ahead of the editor-only source-catalog fallback.
- `engine/framework/include/framework/asset/AssetExecutor.h`, `Asset.h`: replace `tf::Executor`/`tf::AsyncTask` with `sky::ThreadPool`/`sky::TaskNodePtr`; drop the taskflow include from public headers.
- `engine/render/adaptor/src/assets/MaterialAsset.cpp` (and any other path-referencing payload writer): store UUID references in product payloads instead of source paths.
- `engine/framework/include/framework/asset/AssetEvent.h` + `AssetCommon.h`: extend `AssetBuildResult` to carry `uuid`/`target`/`retCode`/`error` so a global build-finished listener can correlate completions (the `Event` callback does not receive the lookup key).
- `engine/framework/.../AssetBuilderManager.*` + the builder-side `AssetTool` (frontend asset browser + background batch-cook worker, built independent of `SKY_BUILD_TOOL`): on-demand cook trigger, LOADING→LOADED/FAILED transition on the build-finished event, and an `ICookRunner` abstraction with in-process and out-of-process (AssetTool worker process + IPC) implementations.
- `engine/framework/...`: an `IAssetDependencyProvider` (forward/reverse/full-graph) assembled in `AssetDataBase` (source dependency index) and `AssetManager` (product headers), with the reverse graph computed by inversion.
- `engine/framework/...`: a source catalog interface (source locator) consumed by the loader — `ResolvePath`, `Exists`, `GetTarget` — the editor injects an `AssetDataBase`-backed implementation, the runtime injects an empty one.
- Project + asset cook configuration files (text, committed): `configs/` project presets and a per-directory asset-level sidecar.
- `engine/framework/.../AssetProductBundle.*` + product header: record the compression codec and decompress on load via the existing `CompressionManager` / `ICompressor` (lz4 registered by `CompressionModule`).
- `engine/framework/include/framework/asset/AssetCommon.h` (`AssetSourceInfo`) + `AssetDataBase.*`: drop the persisted `category` field and its (de)serialization; derive the source `AssetTypeId` from the builder registry.
- Source trees: `engine/assets/**` and workspace `assets/**` gain committed per-directory `assets.jsonl` (generated by a one-time migration pass). Only directories containing registered assets get a manifest.
- `assets.db` becomes a dev/build-side cache (never shipped/read at runtime); a generated product index is shipped inside the product bundle instead.
- Out of scope (follow-ups): incremental/freshness build, load dedup/async hardening, `Uuid::operator<` ordering fix, packaging (`PackageAssetBundle`), legacy/aurora stack convergence.
- Out of scope for **full taskflow removal**: `engine/core` (`core/async/Task.h` `Task`/`TaskExecutor`, `core/async/NamedThread.h` used by terrain/vegetation/navigation), legacy `engine/shader`, and legacy `engine/render/core` (`RenderGraphContext`). Dropping `3rdParty::taskflow` from Core requires a separate change (or rides the legacy stack retirement). Because Core links taskflow PUBLIC, Framework's build-level/transitive detachment is therefore also deferred until that change lands.
