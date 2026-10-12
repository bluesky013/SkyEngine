//
// Content runtime module: registers the content asset data/handlers at runtime
// (no builders). Loaded by the game; the builder module is cook-only.
//

#include <content/adaptor/ContentReflection.h>
#include <framework/interface/IModule.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::content {

    class ContentModule : public IModule {
    public:
        ContentModule()           = default;
        ~ContentModule() override = default;

        bool Init(const StartArguments &args) override
        {
            ContentReflection(SerializationContext::Get());
            return true;
        }

        void Tick(float delta) override
        {
        }
    };

} // namespace sky::content
REGISTER_MODULE_TAGGED(Content_Module, sky::content::ContentModule)
