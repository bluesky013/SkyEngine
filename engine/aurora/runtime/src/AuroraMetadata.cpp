#include <aurora/adaptor/assets/AuroraAssetReflection.h>
#include <framework/interface/IModule.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::aurora {
    class AuroraMetadataModule final : public IModule {
    public:
        void RegisterMeta() override
        {
            RegisterAuroraAssetTypes(SerializationContext::Get());
        }
    };
} // namespace sky::aurora

REGISTER_MODULE_TAGGED(sky::aurora::AuroraMetadataModule)
