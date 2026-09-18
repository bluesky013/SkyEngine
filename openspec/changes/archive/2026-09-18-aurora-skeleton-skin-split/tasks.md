## 1. 类型拆分

- [x] 1.1 新增 `aurora/resource/Skin.h`：自持 `Skin`（`inverseBindMatrices` + `boneMatrices` + `boneMapping`），不依赖动画类型
- [x] 1.2 `resource/Mesh.h`：删除本地 `Bone`/`Skeleton`；只保留 `SetSkin/GetSkin/HasSkin`
- [x] 1.3 `scene/SceneTypes.h`：场景组件 `Skin` → `SkinnedMesh`（持 `CounterPtr<Skin>`），更新 `SKY_TYPE_TAG`
- [x] 1.4 确认 aurora 内不存在 `Skeleton`/`Bone`/`aurora/animation`（animation 为独立模块，映射留给桥接层）

## 2. 文档

- [x] 2.1 `engine/aurora/AGENTS.md` 增加「骨架 / 蒙皮」约定（aurora 无 Skeleton；animation 独立；桥接层映射 animation→Skin；渲染侧调色板用 `SkinningPalette`/`SkinMatrices`）

## 3. 测试

- [x] 3.1 `MeshResourceTest`：改为 skin-only，新增 `SkinAttach` 用例
- [x] 3.2 `SceneCollectTest`：`Skin`→`SkinnedMesh`
- [x] 3.3 回归：`AuroraCoreTest` 32 passed；`AuroraRHITest` 130 passed；`AuroraShaderTest` 40 passed
- [x] 3.4 `openspec validate aurora-skinning --strict` 通过
