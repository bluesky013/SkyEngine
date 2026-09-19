## Context

Legacy render splits LOD into three pieces:

- `render::LodGroup` (`render/core/include/render/lod/LodGroup.h`) — a `RenderResource` holding
  `std::vector<std::unique_ptr<LodProxy>>` plus a `LodConfig { lodBias, scaleFactor }`;
  `SelectLod` computes a projected screen size (via `LodUtils::CalculateScreenSizeSquired`) and
  walks levels from coarsest to finest.
- `LodGroupData` (`render/adaptor/assets/LodGroupAsset.h`) — `{ version, type, levels }` where
  `LodGroupLevelData { float screenSize; Uuid resId; }`; `resId` is a mesh asset.
- `StaticMeshComponent` — its `resId` actually points at a **LodGroup**, and
  `CreateLodGroupFromAsset` loads each level's mesh via `AssetManager` and builds proxies.

Aurora has none of this. Its core resources (`Mesh`, `Material`) are `RefObject`s that carry
`CounterPtr` references (`Mesh` has `CounterPtr<Skin>`, `Material` is not a GPU resource) and stay
framework-free; the adaptor is the only layer that knows about `Uuid` asset references and the
`AssetManager`. `StaticMeshComponentData.mesh` is a single mesh `Uuid`.

## Goals / Non-Goals

**Goals:**
- A core `aurora::LodGroup` resource: ordered levels of `{screenSize, CounterPtr<Mesh>}`, level
  accessors, bounds from level 0, and screen-size-driven `SelectLod`.
- An adaptor `LodGroupAssetData` (versioned) using mesh `Uuid`s, wired into `AssetTraits` /
  `AssetManager` / `AuroraReflection`.
- A dedicated `LodGroupComponent` so LOD entities are distinct from single-mesh ones.
- Keep the `Uuid` ↔ core-type boundary exactly where the existing adaptor puts it.

**Non-Goals:**
- Resolving `LodGroupAssetData` mesh `Uuid`s into `CounterPtr<Mesh>` and building GPU meshes
  (`CreateLodGroupFromAsset` analogue) — deferred to a device-available build change.
- `LodProxy` / skinning proxies / streaming; aurora `LodGroup` is a plain ref-counted value.
- Changing legacy `engine/render/**`.
- A `type` discriminator string, per-level material overrides, or a `LodConfig` bias/scale field
  with editor UI (see Open Questions).

## Decisions

### D1. `aurora::LodGroup` derives `RefObject`, not `RenderResource`

```cpp
namespace sky::aurora {

    struct LodLevel {
        float            screenSize = 1.f;
        CounterPtr<Mesh> mesh;
    };

    class LodGroup : public RefObject {
    public:
        void AddLevel(LodLevel level);
        const std::vector<LodLevel> &GetLevels() const;
        uint32_t GetLevelCount() const;
        CounterPtr<Mesh> GetMesh(uint32_t level) const { ... }   // nullptr when out of range
        const BoundingBoxSphere &GetBoundingSphere() const;      // level 0 mesh bounds
        uint32_t SelectLod(float screenSize) const;
        uint32_t SelectLod(const BoundingBoxSphere &bounds, const Vector3 &viewOrigin, const Matrix4 &proj) const;
    };

} // namespace sky::aurora
```

`LodGroup` owns no GPU resource of its own (it aggregates `Mesh` refs), so `RenderResource`
(which requires `Upload`/`Create`/`Release`) is the wrong base. `Mesh`/`Material` set the
precedent of `RefObject` for high-level resources. Alternative — `RenderResource` — rejected as
it would force meaningless GPU-upload hooks.

`SelectLod(float)` compares a precomputed screen size against each level's `screenSize` threshold
and returns the coarsest level whose threshold is met, defaulting to level 0. The
`BoundingBoxSphere`/projection overload reproduces the legacy `LodUtils` math
(`CalculateScreenSizeSquired`) so callers do not re-implement it; the math lives in the new
`LodGroup.cpp`, no separate `LodUtils` header until a second consumer exists.

### D2. Adaptor `LodGroupAssetData`

```cpp
struct LodGroupLevelData {
    float screenSize = 1.f;
    Uuid  mesh;
};

struct LodGroupAssetData {
    static constexpr uint32_t CURRENT_VERSION = 1;
    uint32_t version = CURRENT_VERSION;
    std::vector<LodGroupLevelData> levels;

    void Save(BinaryOutputArchive &ar) const;
    void Load(BinaryInputArchive &ar);   // rejects version mismatch, clears, logs
};
```

Mirrors legacy `LodGroupLevelData` minus the `type` string (no consumer; the mesh asset already
determines static vs. skinned). Levels are serialized in order with `Uuid` as string. Version
guard follows `ImageAssetData` / `MeshAssetData` so a layout change cannot be mis-parsed.
`AssetTraits<aurora::LodGroup>::ASSET_TYPE = "AuroraLodGroup"`, `SERIALIZE_TYPE = BIN`.

### D3. Dedicated `LodGroupComponent`

```cpp
struct LodGroupComponentData {
    Uuid lodGroup;
    bool castShadow    = false;
    bool receiveShadow = false;
};

class LodGroupComponent : public ComponentAdaptor<LodGroupComponentData>, public IAssetReadyNotifier {
    ...
    SingleAssetHolder<sky::aurora::LodGroup> lodGroupHolder;
};
```

Registered in `ComponentFactory` group `"Aurora"`; the asset member is a `Uuid` tagged
`SET_ASSET_TYPE(AssetTraits<aurora::LodGroup>::ASSET_TYPE)` so the editor `PropertyUuid` picker
works unchanged. Alternatives: an optional `lodGroup` field on `StaticMeshComponent` (two
overlapping mesh paths, ambiguous precedence) and replacing `StaticMeshComponentData.mesh` with a
LodGroup `Uuid` (BREAKING for existing single meshes) — both rejected.

### D4. Uuid ↔ core boundary (deferred build)

`LodGroupAssetData` holds `Uuid` mesh refs (adaptor layer). Populating
`LodGroup::AddLevel(CounterPtr<Mesh>)` requires `AssetManager::LoadAsset<Mesh>` plus the still
missing `CreateMeshFromAsset`; that resolution is a follow-up, exactly as the Mesh/Material/
Texture device build paths are deferred. Until then the component exposes the loaded asset data
and the resource is data-holding.

### D5. Layering

`aurora/core` gains only `LodLevel`/`LodGroup` (no `Uuid`, no framework). `aurora/adaptor` gains
the asset data, traits/handler and component. `AuroraReflection` registers the new asset type and
component (idempotent). The core CMake uses `GLOB_RECURSE`, so new files are picked up at
configure time without CMake edits.

## Risks / Trade-offs

- **Level ordering contract**: `SelectLod` assumes levels are ordered finest-to-coarsest or by
  ascending threshold; an unsorted list gives surprising results. Mitigation: document the
  ordering in `LodGroup` and cover it in a test.
- **Empty levels / null meshes**: `GetBoundingSphere` and `GetMesh` must not dereference null.
  Mitigation: return a static empty sphere and `nullptr`; test both.
- **Binary layout bump**: `LodGroupAssetData` is new, so no migration; the version guard protects
  future changes.
- **Version guard in header vs. .cpp**: to keep `LodGroupAsset.h` light, `Save`/`Load` are
  declared in the header and defined in `src/assets/LodGroupAsset.cpp` (matches
  `LodGroupAsset` legacy and avoids pulling `Logger` into every includer). `MeshAssetData`
  inlines them; either is acceptable, this change chooses the .cpp form.
- **Screen-size math duplication**: the projection formula is copied from legacy `LodUtils`; if a
  later feature needs it too, extract it then rather than now.

## Migration Plan

Pure addition: new core resource, new adaptor asset/component. Rollback = delete the new files and
drop the two `AuroraReflection` registrations.

## Open Questions

- Should `LodGroup` expose a `LodConfig { lodBias; scaleFactor }` like legacy, or keep levels only
  for the first cut? (Current: levels only; `SelectLod` uses no bias/scale.)
- Should the component own a `SingleAssetHolder<aurora::LodGroup>` now, or wait for the build
  path? (Current: holder is wired now; it stays null until the build path lands.)
