## Context

The adaptor mirrors the legacy asset layer (`engine/render/adaptor/assets/MeshAsset.h`), where
`MeshDataHeader` carries both `std::vector<MeshSubSection> subMeshes` and the companion
`std::vector<Uuid> materials`. The aurora port kept only the submesh side:

```cpp
struct MeshSubMeshData {           // engine/aurora/adaptor/.../MeshAsset.h
    uint32_t indexOffset   = 0;
    uint32_t indexCount    = 0;
    uint32_t materialIndex = 0;    // no list to index into
};
struct MeshAssetData {
    std::vector<uint8_t>         vertexData;
    std::vector<uint8_t>         indexData;
    std::vector<MeshSubMeshData> subMeshes;
    AABB                         bounds;
};
```

`MeshSubMeshData.materialIndex` therefore has no referent: nothing maps the index to a material
asset. `StaticMeshComponentData` compounds this by holding a single `Uuid material`, which cannot
express a multi-submesh, multi-material mesh. `aurora::Mesh::SubMesh` (`aurora/core`) keeps
`materialIndex` as a "technique/material placeholder index"; the adaptor is the layer that must
carry the actual asset references.

## Goals / Non-Goals

**Goals:**
- Make the submesh → material reference relationship explicit and serializable.
- Keep one material reference shape across Mesh / Material assets and the component layer.
- Make the binary layout self-describing enough to evolve safely.

**Non-Goals:**
- Changing `aurora/core` `aurora::Mesh` / `SubMesh` (stays asset-reference-free).
- Device-available `MeshAssetData` → `aurora::Mesh` build (`CreateMeshFromAsset`); remains a
  follow-up like the material/texture build paths.
- Named material slots or per-slot sampler/override metadata.
- Offline `AssetBuilderManager` integration.

## Decisions

### D1. Slot table on `MeshAssetData`, not per-submesh `Uuid`

Add the list next to the submeshes:

```cpp
struct MeshAssetData {
    static constexpr uint32_t CURRENT_VERSION = 1;

    uint32_t                     version = CURRENT_VERSION;
    std::vector<uint8_t>         vertexData;
    std::vector<uint8_t>         indexData;
    std::vector<MeshSubMeshData> subMeshes;
    std::vector<Uuid>            materials;   // material slot table
    AABB                         bounds;
};
```

`MeshSubMeshData.materialIndex` indexes `materials`. Rationale: mirrors legacy
`MeshDataHeader.materials`, keeps `MeshSubMeshData` compact for multi-section meshes, and lets
several submeshes share one material (the common case). Alternative — store a `Uuid` directly on
`MeshSubMeshData` — duplicates references and diverges from the legacy contract; rejected. Named
slots add metadata with no current consumer (editor pickers already key off `Uuid` + `ASSET_TYPE`);
rejected for now.

Accessor on `MeshAssetData`:

```cpp
const Uuid *GetMaterialUuid(uint32_t materialIndex) const;  // nullptr when unresolved
```

### D2. `materialIndex` resolution semantics

- `materials` empty → no slot table; every submesh resolves to "no bound material"
  (treated as slot 0 for the slot-0 override rule below).
- `materialIndex < materials.size()` → `materials[materialIndex]`; an all-zero `Uuid` is an
  explicit "no material" slot (valid, resolves to fallback at build time).
- `materialIndex >= materials.size()` (non-empty table) → resolve to slot 0 and log a warning;
  do not fail the whole load, matching the "malformed data should not crash the loader" stance
  used elsewhere in the adaptor.

`Save`/`Load` round-trips the table as strings (same as `MaterialAssetData::shader`).

### D3. Version guard

The layout is changing, so `MeshAssetData` gains `version` + `CURRENT_VERSION`, serialized first.
`Load` rejects `version != CURRENT_VERSION` (leaves data empty, logs) rather than mis-parsing old
or future bytes — consistent with the `ImageAssetData` version guard already in
`aurora-adaptor`. This is a small addition that prevents silent corruption; alternative (no
version, just break the format) rejected because the adaptor already established versioned
assets.

### D4. `StaticMeshComponent` material = slot-0 override

`StaticMeshComponentData` keeps `Uuid material` but redefines it:

- empty `material` → use the mesh asset's slot 0 (the default case, "material comes from the
  asset").
- non-empty `material` → override slot 0 only; submeshes whose `materialIndex != 0` still use the
  asset's `materials[materialIndex]`.

This preserves the existing single-material inspector field (no editor/reflection break) while
removing the ambiguity of "one material for a multi-material mesh". Alternatives: a full
`std::vector<Uuid>` override list (more UI work, no consumer yet) and dropping the field
(breaks the existing picker and simple single-material workflows); both rejected.

The component exposes the resolved material reference to the future device build path; no GPU
resource is created in this change.

### D5. Layering

`MeshAssetData` stays in `Aurora.Adaptor` (framework-linked, `Uuid` asset refs live here), not
`aurora/core`. The core `aurora::Mesh::SubMesh.materialIndex` is unchanged; the eventual build
step copies `materials` into the runtime mesh/feature binding layer.

## Risks / Trade-offs

- **Existing serialized meshes invalidated** → expected: aurora asset loading is runtime-only with
  no shipped products; the version guard makes the failure explicit instead of silent.
- **Index/table drift** (submesh count vs. table use) → resolution is defensive (D2) and covered
  by a round-trip + out-of-range test.
- **Slot-0 override surprise** (a single-material mesh with a component material silently keeps
  other slots' asset materials) → documented in `StaticMeshComponent` and reflected in the spec;
  only slot 0 is implied by the legacy single-material field.
- **Duplicate `Uuid` entries across slots** → allowed (two submeshes sharing a material may
  appear at different indices); no dedup attempted.

## Migration Plan

Additive at the API level plus a binary-format bump. Rollback = revert `MeshAsset.h` /
`StaticMeshComponent.h`; no other module depends on the new field yet.

## Open Questions

- Should the eventual `CreateMeshFromAsset` deduplicate material `Uuid`s into a per-mesh material
  bind list, or keep 1:1 with submeshes? (Deferred to the build-path change.)
- Do light/shadow-only submeshes need an explicit "cast no material" marker, or is an all-zero
  `Uuid` slot sufficient? (Current: all-zero `Uuid` slot.)
