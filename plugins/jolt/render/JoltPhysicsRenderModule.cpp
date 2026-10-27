//
// Created on 2026/10/26.
//

#include "JoltMeshConversion.h"
#include <physics/PhysicsMesh.h>

#include <aurora/adaptor/assets/MeshAsset.h>
#include <framework/application/ModuleManager.h>
#include <framework/asset/AssetManager.h>
#include <framework/interface/IModule.h>

namespace sky::phy {

    class JoltPhysicsRenderModule : public IModule {
    public:
        JoltPhysicsRenderModule()           = default;
        ~JoltPhysicsRenderModule() override = default;

        bool Init(const StartArguments &args) override
        {
            auto *manager = ModuleManager::Current();
            lease         = manager ? manager->RetainModule("JoltPhysicsRenderModule") : nullptr;
            return true;
        }

        void Start() override
        {
            SetPhysicsMeshProvider(
                [](const Uuid &assetUuid) -> CounterPtr<TriangleMesh> {
                    auto asset = AssetManager::Get()->LoadAsset<aurora::Mesh>(assetUuid);
                    if (asset == nullptr) {
                        return nullptr;
                    }
                    asset->BlockUntilLoaded();
                    std::string error;
                    auto        mesh = ConvertAuroraCollisionMesh(*asset, error);
                    if (!mesh)
                        LOG_E("JoltMesh", "%s", error.c_str());
                    return mesh;
                },
                lease, std::string(AssetTraits<aurora::Mesh>::ASSET_TYPE));
        }

        void Shutdown() override
        {
            SetPhysicsMeshProvider(nullptr);
            lease.reset();
        }

    private:
        std::shared_ptr<const void> lease;
    };

} // namespace sky::phy
REGISTER_MODULE_TAGGED(sky::phy::JoltPhysicsRenderModule)
