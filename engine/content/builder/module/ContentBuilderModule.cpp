//
// Content.Metadata owns asset handlers; this module owns builders.
//

#include <content/builder/AnimationBuilder.h>
#include <content/builder/WorldBuilder.h>

#include <framework/asset/AssetBuilderManager.h>
#include <framework/interface/IModule.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::builder {

    class ContentBuilderModule : public IModule {
    public:
        ContentBuilderModule()           = default;
        ~ContentBuilderModule() override = default;

        void RegisterMeta() override
        {
            AssetBuilderManager::Get()->RegisterBuilder(new AnimationBuilder());
            AssetBuilderManager::Get()->RegisterBuilder(new WorldBuilder());
        }

        bool Init(const StartArguments &args) override
        {
            return true;
        }

        void Tick(float delta) override
        {
        }
    };

} // namespace sky::builder
REGISTER_MODULE(sky::builder::ContentBuilderModule)
