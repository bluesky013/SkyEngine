## Why

Offline nav mesh production is the prerequisite for persisting/streaming nav meshes, but it needs infrastructure
that does not exist yet: the repository has no World/Level asset type (the editor loads a world by
`world->LoadJson(json)`), and the nav mesh asset contract (`navigation-mesh-asset-pipeline`) has no producer. This
change adds the offline builder that turns an authored `.navmesh` source into a nav mesh asset.

## What Changes

- Add a framework-level `NaviMeshBuilder : sky::AssetBuilder` registered by a **navigation-owned builder module**
  (`SkyNavigation.Builder`), linking only `Framework` + `Navigation` (no aurora, no direct recast).
- Define the `.navmesh` source asset (JSON): references a level/scene, plus build params (agent, resolution,
  bounds) and the export mode.
- Add the **offline scene/World loading** the builder needs: load the referenced scene into a `World`, attach a
  `NavigationSystem`, apply the authored params, and run the navigation-interface generator synchronously.
- Drive generation through `NaviMeshFactory` / `NaviMeshGenerator` and write the asset via `CollectTiles` +
  `SaveAsset` (backend-agnostic; the recast backend provides tile emission).
- Support the configurable export mode at build time: `Tiled` (per-tile blobs + manifest) or `Full` (single blob).
- Add `Full` runtime load: serialize/restore a monolithic `dtNavMesh`.
- Support **incremental rebuild** of a subset of tiles and verify rebuilt tiles stitch with neighbours.

## Capabilities

### New Capabilities
- `navigation-mesh-builder`: `.navmesh` source, offline scene cook, export-mode selection, full-mesh serialization,
  and incremental tile rebuild.

## Related

- Consumes `navigation-mesh-asset-pipeline` (asset model, interface tile emission, Tiled runtime load).
- `navigation-tile-streaming` / `navigation-async-load` consume builder output at runtime.

## Impact

- New `SkyNavigation.Builder` module + `NaviMeshBuilder`; offline scene/World loading support; the recast backend
  gains `Full` serialization; asset-bundle wiring via `AssetBuilderManager` (not `Aurora.Cook`).

## Open Questions (expand later)

- How the `.navmesh` source references a scene (path vs asset uuid) and where build presets select the export mode.
- Whether `Full` carries a serialized `dtNavMesh` or is rebuilt from the tiled payloads.
