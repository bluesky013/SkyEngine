## Why

`sky::aurora::MeshAssetData` stores per-submesh `MeshSubMeshData.materialIndex` but carries no
material slot list, so a submesh's `materialIndex` cannot be resolved to a material asset. The
submesh → material reference relationship is incomplete, unlike legacy `MeshDataHeader` which
serializes `std::vector<Uuid> materials` alongside its `subMeshes`.

## What Changes

- Add `std::vector<Uuid> materials` to `MeshAssetData`: the material slot table that
  `MeshSubMeshData.materialIndex` indexes into. Serialized in `Save` / `Load`.
- Define `materialIndex` semantics: an index into `materials`; empty table means no material
  binding (treated as slot 0), out-of-range index falls back to slot 0 with a warning.
- Add a `version` field to `MeshAssetData` (`CURRENT_VERSION`); `Load` rejects
  `version != CURRENT_VERSION` instead of silently mis-parsing the new layout.
- Change `StaticMeshComponentData.material` to an optional **slot-0 override**: empty uses the
  mesh asset's slot 0; non-empty overrides slot 0 only (other slots always come from the asset).
- Keep the device-available `MeshAssetData` → `aurora::Mesh` build path as a follow-up; this
  change only completes the asset data + component contract.
- **BREAKING** (internal): the `MeshAssetData` binary layout changes.

## Capabilities

### New Capabilities
- (none)

### Modified Capabilities
- `aurora-adaptor`: mesh asset data gains the material slot reference table and versioned
  load; `StaticMeshComponent` material becomes a slot-0 override.

## Impact

- `engine/aurora/adaptor/include/aurora/adaptor/assets/MeshAsset.h` — `MeshAssetData` fields +
  `Save`/`Load`.
- `engine/aurora/adaptor/include/aurora/adaptor/components/StaticMeshComponent.h` — material
  semantics (docs/accessor), `StaticMeshComponentData`.
- `engine/aurora/adaptor/src/AuroraReflection.cpp` — only if reflection member docs/metadata
  change.
- `engine/aurora/adaptor/test/` — new mesh asset round-trip test (materials + version).
- `openspec/specs/aurora-adaptor/spec.md` — updated on archive.
- `aurora/core` (`aurora::Mesh` / `SubMesh`) unchanged; the adaptor maps its material slots into
  the core resource at build time.
