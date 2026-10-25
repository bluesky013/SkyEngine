#include <aurora/adaptor/AuroraReflection.h>
#include <framework/interface/IModule.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::aurora {
    class AuroraMetadataModule final : public IModule {
    public:
        void RegisterMeta() override
        {
            AuroraReflection(SerializationContext::Get());
        }
    };
} // namespace sky::aurora

REGISTER_MODULE_TAGGED(sky::aurora::AuroraMetadataModule)
