## Context

Legacy bridge (`engine/render/adaptor`) has three parts: (1) `ReflectRenderAsset` — register asset data types + `AssetManager::RegisterAssetHandler<T>()`; (2) per-type `Reflect(SerializationContext*)` — register `Data` structs and components, asset members tagged `SET_ASSET_TYPE(AssetTraits<T>::ASSET_TYPE)`; (3) `ComponentFactory::RegisterComponent<T>(GROUP)`. `RenderModule::Init` runs all three before RHI init. Components hold `Uuid` refs in `Data` and load via `SingleAssetHolder<T>` + `IAssetReadyNotifier`. The editor consumes `ComponentFactory::GetTypes()` (`InspectorWidget`) and `CommonPropertyKey::ASSET_TYPE` (`ReflectedObjectWidget` `PropertyUuid`).

Aurora has none of this. `aurora/core` must stay framework-free, so the bridge is a separate module. There is also a path collision: the launcher module currently owns `engine/aurora/adaptor`.

## Goals / Non-Goals

**Goals:**
- Free `engine/aurora/adaptor` for the bridge; move the launcher module to `engine/aurora/module`.
- Register aurora types, asset handlers and components with the framework from one entry (`AuroraReflection`).
- Runtime asset load path (`AssetTraits` + data + `AssetManager` handler) for `Mesh` / `Material` / `Texture`.
- Asset-backed components (`StaticMesh` + `Light` + `Camera`) visible in the editor with working asset pickers.

**Non-Goals:**
- Offline asset build / `AssetBuilderManager` products (`RenderPrefab`, packed bundles).
- `Skeleton` / `Skin` / animation assets, `Prefab` component.
- Editor UI changes beyond metadata (the existing widgets must work unchanged).

## Decisions

### D1. `adaptor` static lib + `runtime`/`editor` dynamic modules

Mirrors the legacy split (`render/adaptor` static + `SkyRender`/`SkyRender.Editor` dlls):

- `engine/aurora/adaptor` → **`Aurora.Adaptor` (STATIC)**: `AuroraModule` + `AuroraReflection` + `assets/` + `components/`.
- `engine/aurora/runtime` → **`AuroraRender` (SHARED)**: `AuroraRegistry.cpp` only (`REGISTER_MODULE(AuroraModule)`), loaded by the launcher.
- `engine/aurora/editor` → **`AuroraRender.Editor` (SHARED)**: `AuroraEditorModule : AuroraModule` + registry.

`AuroraRender`/`AuroraRender.Editor` link `Aurora.Adaptor`; `Aurora.Adaptor` links `Aurora` + `Framework`. `aurora/core` unchanged (framework-free). Backend dll + `Launcher` deploy dependencies hang off `AuroraRender` (runtime).

### D2. Single entry `sky::aurora::AuroraReflection`

```cpp
void AuroraReflection(sky::SerializationContext *context);
```

Registers, in order: support types → aurora scene types → asset data types + `RegisterAssetHandler` → component `Reflect` + `ComponentFactory`. Idempotent (static guard) because `SerializationContext::Register` asserts on duplicate ids.

### D3. Runtime asset layer (mirrors `render/adaptor/assets`)

- `AssetTraits<sky::aurora::Mesh>` → `MeshAssetData` (vertex/index bytes, submesh ranges, bounds)
- `AssetTraits<sky::aurora::Material>` → `MaterialAssetData` (technique tag + shader uuid + property values/texture uuids)
- `AssetTraits<sky::aurora::Texture>` → `ImageAssetData` (image descriptor + mip/layer payload)
- Each `DataType` gets `Json`/`Bin` save/load registered in `AuroraReflection`; `AssetManager::RegisterAssetHandler<T>()` is called for each.
- The **CPU `DataType` is the asset payload**; turning it into a GPU resource needs a `Device` and is done where a device is available (component `OnAssetLoaded` / a later renderer step), matching legacy where `AssetTraits<T>::DataType` is CPU data and a separate `Create*FromAsset` builds the runtime object.

Runtime-load only: no `AssetBuilderManager` product/packaging integration in this change.

### D4. Components

- `StaticMeshComponent : ComponentAdaptor<StaticMeshComponentData>, IAssetReadyNotifier` — `Data{ Uuid mesh; Uuid material; bool castShadow; bool receiveShadow; }`, two `SingleAssetHolder`s.
- `DirectLightComponent` / `PointLightComponent` / `SpotLightComponent` — split by light type; each `Data` carries only that type's fields (no asset). Direction/position come from the entity transform.
- `CameraComponent` — camera params (fov/near/far/...); no asset.
- All component types live in `sky::aurora` and carry no `Aurora` prefix (the namespace scopes them).
- Asset members reflected with `Uuid` + `SET_ASSET_TYPE(AssetTraits<T>::ASSET_TYPE)`, so the editor `PropertyUuid` picker works. Non-asset components still register with `ComponentFactory`.

### D5. Editor contract

- Components registered under `ComponentFactory` group `"Aurora"`; `InspectorWidget` lists them automatically.
- Asset members MUST be `Uuid` and carry `ASSET_TYPE`; otherwise `PropertyUuid` asserts (`ReflectedObjectWidget.cpp` requires the property).
- **Editor integration is via an editor extension module, not an explicit application call.** Mirror legacy `engine/render/editor`: an aurora editor module (`AuroraEditorModule : AuroraModule`, `REGISTER_MODULE`) is loaded by the editor through the module config; its `Init` runs the base `AuroraModule::Init` (which invokes `AuroraReflection`) and registers editor-side extensions (actor creators / asset creators / previews) through the editor extension mechanisms. The editor therefore discovers aurora components/reflection through the component extension path, exactly like `RenderEditorModule` for legacy render.
- Runtime (launcher) keeps using `AuroraModule::Init`.
- `AssetDataBase` browsing of aurora asset types requires asset-type registration; runtime-load-only keeps `AssetManager::FindAsset` working, DB browse is a follow-up.

> Implementation note: the editor extension module lives under `engine/aurora/editor` (`AuroraRender.Editor`) and depends on the editor framework. It is listed as a follow-up task here so the runtime bridge can land first.

### D6. Layering

`aurora/core` framework-free. `aurora/adaptor` is the inversion point (and where the future animation→Skin mapping can live).

## Risks / Trade-offs

- **Duplicate registration**: guard + ordered registration (support types first).
- **`Name` reflection**: `Name` is interned; serializing as string + resolving via name table (custom Json save/load) unless already supported.
- **`CounterPtr<RefObject>` members** (`SkinnedMesh::skin`): not inline-serializable → `Uuid` asset ref.
- **Asset build needs a device**: `DataType`→resource is deferred to a device-available stage; the first cut may register the data/type without a full GPU build (documented, task-tracked).
- **Editor enumeration order**: if `AuroraReflection` runs after the editor builds its lists, components won't appear → invoke at `Application::Init` time for editor.

## Migration Plan

Additive + one rename (`engine/aurora/adaptor` → `engine/aurora/module`). Rollback = revert rename + drop the new module.

## Open Questions

- Should `DataType`→GPU resource build happen in the component (`OnAssetLoaded`) or a dedicated feature/renderer step? (Current: component/feature when device is available.)
- Camera component: use aurora scene data or a framework-only component? (Current: framework-only component holding camera params.)
