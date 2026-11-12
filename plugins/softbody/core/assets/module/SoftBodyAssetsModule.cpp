#include <framework/interface/IModule.h>
#include <softbody/assets/SoftBodyAsset.h>
#include <softbody/assets/SoftBodyRecipe.h>

namespace sky::softbody {
    class SoftBodyAssetsModule final : public IModule {
    public:
        void RegisterMeta() override
        {
            RegisterSoftBodyAssets();
            RegisterSoftBodyRecipeTypes();
        }
        bool Init(const StartArguments &) override
        {
            return true;
        }
    };
} // namespace sky::softbody
REGISTER_MODULE_TAGGED(sky::softbody::SoftBodyAssetsModule)
