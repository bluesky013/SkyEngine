#include <builder/physics/PhysicsAssetBuilder.h>
#include <framework/asset/AssetBuilderManager.h>
#include <framework/interface/IModule.h>

namespace sky::phy {
    class PhysicsBuilderModule final : public IModule {
    public:
        void RegisterMeta() override
        {
            AssetBuilderManager::Get()->RegisterBuilder(new PhysicsAssetBuilder());
        }
        bool Init(const StartArguments &) override
        {
            return true;
        }
    };
} // namespace sky::phy
REGISTER_MODULE_TAGGED(sky::phy::PhysicsBuilderModule)
