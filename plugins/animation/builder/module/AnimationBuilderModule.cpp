#include <animation/builder/GraphBuilder.h>
#include <framework/asset/AssetBuilderManager.h>
#include <framework/interface/IModule.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::animation {
    class AnimationBuilderModule final : public IModule {
    public:
        void RegisterMeta() override
        {
            AssetBuilderManager::Get()->RegisterBuilder(new GraphBuilder());
        }
        bool Init(const StartArguments &) override
        {
            return true;
        }
    };
} // namespace sky::animation
REGISTER_MODULE_TAGGED(sky::animation::AnimationBuilderModule)
