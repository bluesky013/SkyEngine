## Context

`navigation-mesh-asset-pipeline` defines the nav mesh asset payload, interface tile emission, and the Tiled runtime
load. This change adds the offline producer. The framework `AssetBuilder` is file-oriented
(`AssetBuildRequest { FilePtr file; AssetSourcePtr assetInfo; ProductBundleKey target; }`), while nav mesh
generation needs scene collision geometry, so the builder is driven by an authored `.navmesh` source that
references a scene.

## Goals / Non-Goals

**Goals**
- A navigation-owned `NaviMeshBuilder` (framework `AssetBuilder`) registered by `SkyNavigation.Builder`, linking
  only `Framework` + `Navigation` (no aurora, no direct recast).
- A `.navmesh` source describing scene, build params, and export mode.
- Offline scene cook: load the scene into a `World`, attach `NavigationSystem`, generate, and write the asset.
- Tiled and Full export; Full serialize/restore; subset rebuild.

**Non-Goals**
- Runtime tile paging/streaming and async loading/querying (separate backlog changes).
- Editor authoring UI for the `.navmesh` source.

## Decisions

### D1. File-oriented source referencing a scene
`NaviMeshSourceData` (`.navmesh`) carries a workspace-relative scene path plus agent/resolution/bounds params and
an export mode. The builder resolves it via `AssetDataBase::Get()->GetWorkSpaceFs()` and `World::LoadJson`.

### D2. Offline cook through the interface
`NaviMeshBuilder` creates a `World`, attaches `NavigationSystem`, applies the authored params, then drives
`NaviMeshFactory::CreateGenerator()` + `StartAsync()` + `TaskExecutor::WaitForAll()` and `CollectTiles`. It links
no recast types; the loaded backend plugin provides the generator.

### D3. Export mode is a builder input
`NaviMeshGenerator::SetExportMode` selects the payload. `Tiled` snapshots per-tile blobs **before** ownership is
transferred to the tile cache; `Full` serializes the built `dtNavMesh` after the build.

### D4. Full serialization format
`RecastNaviMesh::Serialize/Deserialize` use a `NavMeshSetHeader { version, tileCount, dtNavMeshParams }` followed by
per-tile `NavMeshTileHeader { tileRef, dataSize }` and tile data, matching the Detour sample layout. `LoadData`
dispatches to `Deserialize` for `Full`.

### D5. Incremental rebuild
`NaviMeshGenerator::SetRebuildTiles` restricts the build to a subset; `NaviMesh::RemoveTile` removes a tile from
both the nav mesh and its tile cache, so an edited region can be rebuilt and re-stitched.

## Risks / Trade-offs

- [Full load needs a valid serialized blob] -> versioned header; `Deserialize` rejects unknown versions/bad sizes.
- [`getTile(int)` overload resolution] -> tile enumeration uses a `const dtNavMesh*` so the public const overload
  is selected.
- [Offline cook depends on scene/component reflection being registered by loaded modules] -> the builder runs in the
  asset-build tooling where the physics/backend modules are loaded.
- [Verification of stitched borders and real queries needs a scene] -> deferred; see Open Questions.

## Open Questions

- Integration verification (stitching, full-load query, scene-level tests) needs an asset-build harness with a
  sample scene; tracked as the remaining builder tasks.
