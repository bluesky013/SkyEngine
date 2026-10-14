## 1. Framework

- [x] 1.1 Add `IModule::RegisterMeta()` (process-once phase) with the "persistent registration belongs here" contract
- [x] 1.2 `ModuleManager::LoadModules` runs `RegisterMeta` once per module (process-global `Once`, key
      `ModuleMeta:<name>`) before `Init`, for both static and dynamic modules

## 2. Migrate persistent registrations to `RegisterMeta`

- [x] 2.1 `plugins/recast` - `NaviMeshData`/`NavigationSubSystemConfig` reflection + `NaviMesh` asset handler
- [x] 2.2 `plugins/bullet` - `PhysicsBodyComponent`/`PhysicsSubSystemConfig` reflection (backend/subsystem stay in Start)
- [x] 2.3 `plugins/audio` - `AudioRegistry`/`AudioSourceComponent`/`AudioListenerComponent` reflection + `AudioClip`
      handler (factory/engine stay in Start) and `plugins/audio/builder`
- [x] 2.4 `plugins/terrain` runtime (`TerrainComponent` + terrain asset type) and builder (`TerrainSourceData` + builder)
- [x] 2.5 `plugins/vegetation` runtime (`VegetationComponent` + vegetation asset type)
- [x] 2.6 `engine/content` runtime (`ContentReflection`) and builder (`ContentReflection` + `AnimationBuilder`)
- [x] 2.7 `engine/aurora/adaptor` `AuroraModule` (`AuroraReflection`) and `engine/aurora/cook` (`AuroraReflection` + builders)
- [x] 2.8 `engine/navigation/builder` (`NaviMeshSourceData` + `NaviMeshBuilder`)

## 3. Revert idempotency patches

- [x] 3.1 Revert `SerializationContext::Register` to the strict assertion
- [x] 3.2 Revert `ComponentFactory::RegisterComponent`, `CompressionManager::Register`, and
      `AssetBuilderManager::RegisterBuilder` to their originals (no residual diff)

## 4. Verify

- [x] 4.1 Build all targets
- [x] 4.2 Run the full `ctest` suite - 33/33, including `RecastModuleTest.ReloadAfterUnload`
- [x] 4.3 Confirm the legacy render stack (`engine/render`, `engine/shader`) is untouched
