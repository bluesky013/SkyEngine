//
// Bullet physics <-> render bridge module.
//
// Physics triangle-mesh collision needs engine TriangleMesh data. Aurora's mesh asset carries a
// render-agnostic CPU payload (vertex/index bytes), so this module converts it into physics
// TriangleMesh data and registers the provider with the interface-only physics core.
//
// Layering: engine/physics (interfaces/data) <- plugins/bullet (backend) <- this render bridge
// (plugin submodule). No legacy render dependency.
//

#include <physics/PhysicsMesh.h>

#include <aurora/adaptor/assets/MeshAsset.h>
#include <framework/asset/AssetManager.h>
#include <framework/interface/IModule.h>

namespace sky::phy {

    namespace {

        CounterPtr<TriangleMesh> ConvertAuroraMesh(const aurora::MeshAssetData &data)
        {
            if (data.vertexData.empty() || data.indexData.empty()) {
                return nullptr;
            }

            auto *mesh = new TriangleMesh();
            mesh->vtxStride = sizeof(Vector3);
            mesh->position  = data.vertexData;
            mesh->indexType = IndexType::U32;
            mesh->indexRaw  = data.indexData;

            const uint32_t vertexCount = static_cast<uint32_t>(mesh->position.size() / mesh->vtxStride);
            for (const auto &sub : data.subMeshes) {
                mesh->AddView(0, vertexCount, sub.indexOffset, sub.indexCount, data.bounds);
            }

            return mesh;
        }

    } // namespace

    class BulletPhysicsRenderModule : public IModule {
    public:
        BulletPhysicsRenderModule()           = default;
        ~BulletPhysicsRenderModule() override = default;

        bool Init(const StartArguments &args) override { return true; }

        void Start() override
        {
            SetPhysicsMeshProvider([](const Uuid &assetUuid) -> CounterPtr<TriangleMesh> {
                auto asset = AssetManager::Get()->LoadAsset<aurora::Mesh>(assetUuid);
                if (asset == nullptr) {
                    return nullptr;
                }
                asset->BlockUntilLoaded();
                return ConvertAuroraMesh(asset->Data());
            });
        }

        void Shutdown() override
        {
            SetPhysicsMeshProvider(nullptr);
        }
    };

} // namespace sky::phy
REGISTER_MODULE(sky::phy::BulletPhysicsRenderModule)
