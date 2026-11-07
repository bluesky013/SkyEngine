#include <framework/interface/IModule.h>
#include <framework/serialization/SerializationContext.h>
#include <particle/assets/ParticleEffectAsset.h>

namespace sky::particle {
    class ParticleAssetsModule final : public IModule {
    public:
        void RegisterMeta() override
        {
            RegisterParticleAssets(SerializationContext::Get());
        }
    };
} // namespace sky::particle
REGISTER_MODULE_TAGGED(sky::particle::ParticleAssetsModule)
