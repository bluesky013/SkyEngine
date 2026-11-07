#include <framework/interface/IModule.h>
#include <particle/builder/ParticleEffectBuilder.h>

namespace sky::particle {
    class ParticleBuilderModule final : public IModule {
    public:
        void RegisterMeta() override
        {
            RegisterParticleBuilder();
        }
    };
} // namespace sky::particle
REGISTER_MODULE_TAGGED(sky::particle::ParticleBuilderModule)
