//
// Content builder module: registers the content asset builders with the
// framework offline builder. Cook-only; the runtime module registers handlers.
//

#include <content/adaptor/ContentReflection.h>
#include <content/builder/AnimationBuilder.h>

#include <framework/asset/AssetBuilderManager.h>
#include <framework/interface/IModule.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::builder {

    class ContentBuilderModule : public IModule {
    public:
        ContentBuilderModule()           = default;
        ~ContentBuilderModule() override = default;

        bool Init(const StartArguments &args) override
        {
            ContentReflection(SerializationContext::Get());
            AssetBuilderManager::Get()->RegisterBuilder(new AnimationBuilder());
            return true;
        }

        void Tick(float delta) override
        {
        }
    };

} // namespace sky::builder
REGISTER_MODULE(sky::builder::ContentBuilderModule)
