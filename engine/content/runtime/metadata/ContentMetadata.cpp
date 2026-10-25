#include <content/adaptor/ContentReflection.h>
#include <framework/interface/IModule.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::content {
    class ContentMetadataModule final : public IModule {
    public:
        void RegisterMeta() override
        {
            ContentReflection(SerializationContext::Get());
        }
    };
} // namespace sky::content

REGISTER_MODULE_TAGGED(sky::content::ContentMetadataModule)
