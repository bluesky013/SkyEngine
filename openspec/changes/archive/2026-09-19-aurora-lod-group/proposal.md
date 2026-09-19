## Why

The aurora stack has no level-of-detail model: a mesh instance is a single
`aurora::Mesh`, and the adaptor's `StaticMeshComponentData.mesh` points at exactly one mesh asset.
Legacy render has a full LOD path (`render::LodGroup` + `render/adaptor/assets/LodGroupAsset` with
versioned `LodGroupLevelData { screenSize; resId; }` levels selected by projected screen size).
Aurora needs the same, split across its layers: a core `aurora::LodGroup` resource and an adaptor
asset/component binding.

## What Changes

- **Core resource**: add `aurora/resource/LodGroup.h` (`sky::aurora::LodGroup : RefObject`) with
  ordered `LodLevel { float screenSize; CounterPtr<Mesh> mesh; }` entries, level accessors, a
  bounds/sphere `GetBoundingSphere` derived from level 0, and `SelectLod` (threshold selection
  plus a projected-screen-size overload mirroring legacy `LodUtils`).
- **Adaptor asset**: add `aurora/adaptor/assets/LodGroupAsset.h` — versioned `LodGroupAssetData`
  (`std::vector<LodGroupLevelData> levels`, `LodGroupLevelData { float screenSize; Uuid mesh; }`)
  with `Save`/`Load`, `AssetTraits<aurora::LodGroup>` (`ASSET_TYPE = "AuroraLodGroup"`, `BIN`) and
  `AssetManager::RegisterAssetHandler<aurora::LodGroup>()` in `AuroraReflection`.
- **Component**: add `sky::aurora::LodGroupComponent` (`LodGroupComponentData { Uuid lodGroup;
  castShadow; receiveShadow; }`, `SingleAssetHolder<aurora::LodGroup>`, `IAssetReadyNotifier`),
  reflected with a `Uuid` + `SET_ASSET_TYPE` member and registered with `ComponentFactory` group
  `"Aurora"`. `StaticMeshComponent` is unchanged.
- Device-available resolution of `LodGroupAssetData` mesh `Uuid`s into `CounterPtr<Mesh>` levels
  (the `CreateLodGroupFromAsset` analogue) stays a follow-up, consistent with the deferred
  Mesh/Material/Texture build paths.

## Capabilities

### New Capabilities
- `aurora-lod-group`: the `aurora::LodGroup` core resource (levels, bounds, `SelectLod`), the
  adaptor `LodGroupAssetData` format, and the `LodGroupComponent` binding.

### Modified Capabilities
- `aurora-adaptor`: the runtime asset layer now also covers `aurora::LodGroup`, and the component
  set gains `LodGroupComponent`.

## Impact

- New: `engine/aurora/core/include/aurora/resource/LodGroup.h` (+ `src/resource/LodGroup.cpp`).
- New: `engine/aurora/adaptor/include/aurora/adaptor/assets/LodGroupAsset.h`,
  `engine/aurora/adaptor/include/aurora/adaptor/components/LodGroupComponent.h`.
- Modified: `engine/aurora/adaptor/src/AuroraReflection.cpp` (asset type + handler + component).
- New tests under `engine/aurora/core/test/` (selection) and `engine/aurora/adaptor/test/`
  (asset round-trip / version guard).
- `aurora/core` stays framework-free; `LodGroup` holds `CounterPtr<Mesh>` only, no `Uuid`.
- Legacy `engine/render/**` LOD code is untouched (reference only).
