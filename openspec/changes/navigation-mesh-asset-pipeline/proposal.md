## Why

The nav mesh is rebuilt at runtime and never persisted, so there is nothing to stream. Crucially, tile-based
streaming cannot be bolted on at runtime only: the build/generation stage must emit tiles as **independently
addressable, persistable units** with a manifest and fixed build parameters. If generation produces a monolithic
in-memory mesh, later paging, async loading and dynamic rebuilds are impossible. This change makes the offline
builder tile-aware from the start.

## What Changes

- Add a framework-level `NaviMeshBuilder : sky::AssetBuilder`, registered by a **navigation-owned builder module**
  (`SkyNavigation.Builder`), following the `SkyRender.Builder` / `SkyAudio.Builder` precedent.
- The builder links **only `framework` + `navigation` (the interface module)** and drives generation through the
  navigation interface (`NaviMeshFactory` / `NaviMeshGenerator`), so it is **backend-agnostic**: no direct recast
  link and no aurora dependency. Backend-specific tile emission sits behind the interface.
- Extend the navigation interface with a per-tile payload emission entry so the builder never touches recast types.
- **Resolved input model**: the builder is driven by a `.navmesh` source asset (JSON) that references a
  level/scene asset plus build params and the export mode. `NaviMeshBuilder::Request` loads the referenced
  scene's collision geometry offline (no running `World`), generates through the navigation interface, and writes
  the nav mesh asset. This fits the framework `AssetBuilder` file-oriented contract
  (`AssetBuildRequest { FilePtr file; AssetSourcePtr assetInfo; ProductBundleKey target; }`).
- Support a **configurable export mode** selected at build time:
  - `Tiled`: per-tile payloads + manifest (streaming-ready; required for large/open worlds).
  - `Full`: a single monolithic nav mesh blob (small levels / no streaming; simplest to load).
  The same asset type/version tag covers both; the mode is recorded so the runtime picks the right load path.
- Generate **per-tile** payloads in `Tiled` mode: each nav mesh tile blob and its tile-cache layer blobs are stored
  as separate, addressable units keyed by `(tx, ty, layer)`.
- Emit a **manifest/index** alongside the tiles: tile coordinates, bounds, layer count, per-tile data offset/size,
  and border/connectivity info, so a pager can load a subset without reading the whole asset.
- Persist the **fixed build parameters** required to reconstruct the runtime `dtNavMeshParams` identically: agent
  config, resolution (cell size / cell height / tile size), grid origin, world bounds, and a format/version tag.
- Keep `borderSize` handling consistent so neighbouring tiles stitch across borders.
- Support **incremental rebuild** of a subset of tiles (world edit) rather than rebuilding everything.
- Provide a runtime load entry that adds tiles from the asset into a `dtNavMesh`/tile cache, handling both
  `Tiled` and `Full` exports.

## Capabilities

### New Capabilities
- `navigation-mesh-asset-pipeline`: tile-aware offline nav mesh builder, tile payload layout, manifest, build
  params, and runtime tile load entry.

## Prerequisite / related

- **Prerequisite for `navigation-tile-streaming`**: streaming paging consumes the per-tile payloads + manifest
  defined here. Tile addressing and manifest shape must be fixed before paging is implemented.
- `navigation-async-load`: the off-thread mechanism that reads these tiles.
- Existing `fix-navigation-recast` provides the render-agnostic core this builds on.

## Impact

- New `SkyNavigation.Builder` module + `NaviMeshBuilder` (framework `AssetBuilder`), linking only `Framework` +
  `Navigation`; `engine/navigation` asset types + an interface-level tile emission entry; asset-bundle wiring via
  `AssetBuilderManager` (not `Aurora.Cook`). The recast backend provides tile emission behind the interface.

## Open Questions (expand later)

- Export mode default and how it is selected (asset-build preset vs per-asset setting); whether `Full` can be
  re-exported to `Tiled` without a source rebuild.
- One nav mesh asset with a tile manifest vs one asset per tile (load granularity vs asset-count overhead).
- Whether to store an uncompressed fast-load tile cache in addition to the compressed tile-cache layers.
- Manifest versioning/migration and how grid origin/bounds are authored.
