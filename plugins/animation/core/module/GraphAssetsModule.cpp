#include <animation/assets/GraphAsset.h>
#include <framework/interface/IModule.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::animation {
    class GraphAssetsModule final : public IModule {
    public:
        void RegisterMeta() override
        {
            RegisterGraphAssets(SerializationContext::Get());
        }
    };
} // namespace sky::animation

REGISTER_MODULE_TAGGED(sky::animation::GraphAssetsModule)
