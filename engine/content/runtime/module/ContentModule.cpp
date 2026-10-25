//
// Shared asset schema and handlers are owned by Content.Metadata.
//

#include <framework/interface/IModule.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::content {

    class ContentModule : public IModule {
    public:
        ContentModule()           = default;
        ~ContentModule() override = default;



        bool Init(const StartArguments &args) override
        {
            return true;
        }

        void Tick(float delta) override
        {
        }
    };

} // namespace sky::content
REGISTER_MODULE_TAGGED(sky::content::ContentModule)
