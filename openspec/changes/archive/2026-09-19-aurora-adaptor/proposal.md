## Why

Aurora has no bridge to the framework asset/component layer. Legacy render provides one in `engine/render/adaptor`: `ReflectRenderAsset` registers asset data types + `AssetManager` handlers, `RegisterComponents` reflects components (with `SET_ASSET_TYPE` asset members) and registers them with `ComponentFactory`, and asset-backed components (`StaticMeshComponent`, `LightComponent`, ...) hold `Uuid` refs loaded via `SingleAssetHolder`. The editor (`InspectorWidget` / `ReflectedObjectWidget`) consumes exactly that metadata.

Aurora's new stack has none of this, and it cannot live in `aurora/core` (which must stay framework-free). The launcher module already lives in `engine/aurora/adaptor`; the bridge joins it there (same directory, separate target), matching legacy layout.

## What Changes

- **Single `engine/aurora/adaptor` directory** hosting both targets, mirroring legacy `render/adaptor`: existing launcher module (`AuroraRender`, SHARED) **and** new bridge static lib **`Aurora.Adaptor`** (links `Aurora` + `Framework`). No path rename.
- **Runtime asset layer**: `AssetTraits<aurora::Mesh / Material / Texture>` + asset data structs with `Json`/`Bin` save/load → build the aurora resource, and `AssetManager::RegisterAssetHandler<T>()`. Runtime-load only (no offline `AssetBuilderManager`).
- **Reflection entry** `void sky::aurora::AuroraReflection(sky::SerializationContext *context)` (mirrors `sky::CoreReflection`): registers types, asset handlers and components; idempotent.
- **Asset-backed components** (namespace `sky::aurora`, no `Aurora` prefix): `StaticMeshComponent` (`Uuid` mesh + material), `DirectLightComponent` / `PointLightComponent` / `SpotLightComponent` (split by light type), `CameraComponent`; `ComponentAdaptor<Data>` + `IAssetReadyNotifier`; asset members tagged `SET_ASSET_TYPE`, registered with `ComponentFactory` group `"Aurora"`.
- **Editor integration**: via an editor extension module (`AuroraEditorModule : AuroraModule` + `REGISTER_MODULE`, loaded by the editor module config), not an application-init call; `Uuid` + `ASSET_TYPE` members drive the `PropertyUuid` asset picker.
- **Invocation**: `AuroraModule::Init` calls `AuroraReflection(SerializationContext::Get())`; the editor extension module inherits that `Init`.

## Capabilities

### New Capabilities
- `aurora-adaptor`: the aurora→framework asset/reflection/component bridge and its editor contract.

### Modified Capabilities
- (none)

## Impact

- Extended: `engine/aurora/adaptor/` gains `AuroraReflection` + `assets/` + `components/` and the `Aurora.Adaptor` static target, alongside the existing `AuroraRender` module.
- Modified: `engine/aurora/adaptor/CMakeLists.txt` (two targets); `AuroraRender` links `Aurora.Adaptor` and invokes `AuroraReflection`.
- `aurora/core` stays framework-free.
- First scope: `StaticMesh` + `Material` + `Texture`; `Light` / `Camera` components. `Skeleton`/`Skin` assets, `Prefab`, and offline asset build are follow-ups.
