## Why

Aurora has no bridge to the framework asset/component layer. Legacy render provides one in `engine/render/adaptor`: `ReflectRenderAsset` registers asset data types + `AssetManager` handlers, `RegisterComponents` reflects components (with `SET_ASSET_TYPE` asset members) and registers them with `ComponentFactory`, and asset-backed components (`StaticMeshComponent`, `LightComponent`, ...) hold `Uuid` refs loaded via `SingleAssetHolder`. The editor (`InspectorWidget` / `ReflectedObjectWidget`) consumes exactly that metadata.

Aurora's new stack has none of this, and it cannot live in `aurora/core` (which must stay framework-free).

## What Changes

- **`engine/aurora/adaptor` is a pure static lib** `Aurora.Adaptor` (bridge: `AuroraModule` + `AuroraReflection` + assets + components). The dynamic modules are split out: **`engine/aurora/runtime`** → `AuroraRender` (SHARED, registers `AuroraModule`) and **`engine/aurora/editor`** → `AuroraRender.Editor` (SHARED, `AuroraEditorModule : AuroraModule`), mirroring the legacy `render/adaptor` (static) + `SkyRender`/`SkyRender.Editor` (dll) split.
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

- `engine/aurora/adaptor/` = pure static `Aurora.Adaptor` (`AuroraModule` + `AuroraReflection` + `assets/` + `components/`).
- New: `engine/aurora/runtime/` → `AuroraRender` (SHARED, registry only); `engine/aurora/editor/` → `AuroraRender.Editor` (SHARED). Both link `Aurora.Adaptor`.
- Modified: `engine/aurora/CMakeLists.txt`; `AuroraModule::Init` invokes `AuroraReflection`.
- `aurora/core` stays framework-free.
- First scope: `StaticMesh` + `Material` + `Texture`; `Light` / `Camera` components. `Skeleton`/`Skin` assets, `Prefab`, and offline asset build are follow-ups.
