---
title: "Asset Pipeline"
description: "Source identities, the mounted source namespace, product bundles, loading, cook configuration and the dependency graph."
module: "framework"
updated: "2026-10-09"
---

## Overview

The asset pipeline separates **source assets** (authoring files) from **product assets** (cooked outputs). References to
assets are always `Uuid`; the pipeline resolves a stable identity once, cooks sources into per-platform products, and
loads only products at runtime and in the editor.

Two planes participate:

- **Authoring plane** (`AssetDataBase`): source catalog, manifests, mutation, build entry. Editor/build only.
- **Loading plane** (`AssetManager`): product bundles, product index, deserialization. Editor and runtime.

The loader never reads source data; it only queries a narrow source-catalog interface to decide whether to cook.

## Source identity: `assets.jsonl`

Each physical source directory (per mount) carries an `assets.jsonl` manifest committed to version control:

```jsonl
{"file":"rock.png","id":"3f9a1c2e-4b8d-4a11-a1b2-2f6e9c0d8e77","cook":{"targets":["pc_bc"]}}
{"file":"wood.png","id":"0a1b2c3d-4e5f-6789-abcd-ef0123456789"}
```

- JSON Lines: one JSON object per line, sorted by `file`, UTF-8 / LF. One object per line keeps merges line-based.
- `file` is the directory-scoped key; `id` is the stable `Uuid`; `cook` is the optional asset cook block.
- `AssetDataBase::RegisterAsset` resolves the UUID from the manifest (a hit wins), seeds a legacy value when
  migrating, or generates `Uuid::Create()` for a new asset. Source files with no manifest entry get a fresh identity.
- `AssetIndexFile` / `AssetIndexFileCache` implement parse/serialize, atomic save (temp + rename) and a parsed cache.

## Mounted source namespace

Source roots are ordered mounts composed in a `MultiFileSystem`:

- **workspace** (writable) is mounted first; **engine** (read-only) second; custom/pak/DLC add more.
- The asset key is a single logical `FilePath path`; earlier mounts shadow later ones.
- Manifests are per physical directory per mount; resolving a logical path reads the owning mount's manifest.

## Asset type identity

The type is a single `AssetTypeId` string shared by the builder registry, the handler registry and the product header.

- Source side: `AssetDataBase::QueryType(ext)` derives the type from the file extension via `AssetBuilderManager`.
- Product side: the product header records the type; the loader selects the handler by it.
- No `category` is persisted on the source record.

## Product bundles and layout

```
<workspace>/
|-- assets/                     # source tree (mounts; each dir has assets.jsonl)
`-- products/
    |-- common/                 # cross-platform bundle
    |   |-- product.index       # JSON Lines {"path","id"}
    |   `-- 3f/3f9a1c2e-....bin
    `-- tex_pc/                 # platform bundle
        |-- product.index
        `-- ...
```

- Product payloads are `<uuid[0:2]>/<uuid>.bin`, keyed by UUID; the header carries type, dependencies and codec.
- `AssetIndexFile` (keyed on `path`) maps a canonical logical path (`/` separators) to a UUID. It is platform-independent; only bundle
  membership varies by preset.

## Two-level path resolution

`AssetManager::LoadAssetFromPath` resolves a path to a UUID:

1. **Product index** (level 1, shipped): merged across all added bundles.
2. **Source catalog** (`ISourceCatalog`, level 2, editor only): `ResolvePath` / `Exists` / `GetType` / `GetTarget` /
   `GetSourcePath`, backed by `AssetDataBase`; the runtime injects an empty catalog.

## Loading and on-demand cook

`AssetManager::LoadAsset(uuid)`:

1. **Product exists** -> deserialize on the asset thread pool, loading dependencies first.
2. **Product missing + source exists** (editor) -> create the typed asset, return it in **LOADING**, and schedule an
   in-process cook on a dedicated cook pool (coalesced per UUID). On success the asset becomes **LOADED**; on failure it
   becomes **FAILED** and a later load re-attempts.
3. **Product missing + no source / runtime** -> hard error.

The wait handle (a future) is established before the cook is scheduled so `BlockUntilLoaded` unblocks on
LOADED/FAILED. `BlockUntilLoaded` must not be called from the executor's loader or cook pool threads.

## Cook configuration

`CookConfig` resolves the effective target for an asset:

- Project-level `configs/asset_cook.jsonc` (JSONC): `platforms` (platform -> target) and `targets`
  (target -> bundle + per-kind settings).
- Asset-level `cook` block in `assets.jsonl`: `targets`, per-kind overrides and a free-form `user` block.
- Effective target = asset override x project preset x current platform, defaulting to the primary bundle `common`.

## Dependency graph

`IAssetDependencyProvider` exposes forward `Dependencies`, reverse `Dependents` (by inversion) and full-graph
iteration. `AssetDependencyGraph` is the concrete store. Forward edges come from product headers at runtime and from
`AssetSourceInfo::dependencies` in the editor; dependencies are not stored in `assets.jsonl` or `product.index`.

## Async execution

`AssetExecutor` owns two `sky::ThreadPool` instances: the **loader pool** for deserialization and the **cook pool** for
in-process cooking, so a cook never occupies a loader worker. `Asset::AsyncTask` is a task node plus a future.
`Framework`'s public headers no longer expose taskflow.

## Source-asset mutation

`AssetDataBase` provides framework mutation APIs (editor UI wiring is deferred):

- `MoveAsset(from, to)` keeps the UUID and relocates the manifest entry.
- `DuplicateAsset(from, to)` copies the file and assigns a new UUID.
- `RemoveAsset(id)` removes the file's manifest entry and identity; products are reclaimed by a later build.

## Source identity and legacy migration

Source identity lives in the per-directory **manifest** (`assets.jsonl`, JSON Lines `{"file","id"[,"cook"]}`,
sorted, committed). `assets.db` is a regenerable dev cache and an optional cross-check, never the source of truth.

Because the old identity was derived from `(bundle, path)`, the migration **reproduces the legacy UUID exactly**:

```cpp
// legacy: Uuid::CreateWithSeed(HashCombine32(bundle, Fnv1a32(path)))
// legacy SourceAssetBundle ordinals: INVALID=0, ENGINE=1, WORKSPACE=2
uint32_t hash = 0;
HashCombine32(hash, bundle);          // mount -> legacy role (engine -> ENGINE, else WORKSPACE)
HashCombine32(hash, Fnv1a32(path));
uuid = Uuid::CreateWithSeed(hash);
```

- `AssetDataBase::MigrateLegacyIdentity()` scans every mount (builder-known extensions **plus `.world`**) and seeds
  each directory manifest with the legacy UUID. Existing manifest entries and `assets.db` rows win, so it is
  **idempotent**; only directories containing assets get a manifest.
- Entry point: `AssetTool --project <dir> --engine <dir> --migrate` (one-shot, then exits).
- **Rollback**: delete the manifests; path-derived identity (`CalculateUuidByPath`) resumes.

## Key types

| Type | Role |
|---|---|
| `AssetDataBase` | Source catalog, manifests, mutation, build entry |
| `ISourceCatalog` | Narrow editor-side query interface used by the loader |
| `AssetIndexFile` / `AssetIndexFileCache` | Generic keyed JSON-Lines index file + parsed cache (backs `assets.jsonl` and `product.index`) |
| `AssetManager` | Product bundles, product index, loading |
| `CookConfig` | Project + asset cook target resolution |
| `AssetDependencyGraph` | Forward/reverse dependency queries |
| `AssetExecutor` | Loader/cook thread pools |

## Limitations

- `assets.db` is rebuilt by scanning every mount for builder-known extensions plus `.world`; engine assets
  without a committed manifest fall back to path-derived identity.
- Out-of-process cooking is wired (the `AssetTool` background worker) and the asset-browser frontend triggers cooks,
  including batch (`Cook All Sources`). Single-file packaging remains a follow-up.
