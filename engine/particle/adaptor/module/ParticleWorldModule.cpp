#include <framework/interface/IModule.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/world/WorldSubSystemRegistry.h>
#include <particle/world/ParticleComponent.h>
#include <particle/world/ParticleSystem.h>

namespace sky::particle {
    namespace {
        void ReflectConfig(SerializationContext *context)
        {
            context->Register<ParticleWorldConfig>("ParticleWorldConfig")
                .Member<&ParticleWorldConfig::maxParticles>("maxParticles")
                .Member<&ParticleWorldConfig::simulate>("simulate")
                .Member<&ParticleWorldConfig::lodStart>("lodStart")
                .Member<&ParticleWorldConfig::lodEnd>("lodEnd")
                .Member<&ParticleWorldConfig::lodMinRate>("lodMinRate")
                .Member<&ParticleWorldConfig::cullDistance>("cullDistance");
        }
    } // namespace

    class ParticleWorldModule final : public IModule {
    public:
        void RegisterMeta() override
        {
            ParticleComponent::Reflect(SerializationContext::Get());
            ReflectConfig(SerializationContext::Get());
        }
        bool Init(const StartArguments &) override
        {
            if (WorldSubSystemRegistry::Get().IsRegistered(Name(ParticleSystem::NAME.data())))
                return false;
            WorldSubSystemRegistration registration;
            registration.factory = [](World &, const Any &config) -> std::unique_ptr<IWorldSubSystem> {
                auto system = std::make_unique<ParticleSystem>();
                if (const auto *value = config.GetAsConst<ParticleWorldConfig>(); value && !system->SetConfig(*value))
                    return {};
                return system;
            };
            registration.configType        = TypeInfoObj<ParticleWorldConfig>::Get()->RtInfo();
            registration.makeDefaultConfig = [] { return Any(std::in_place_type<ParticleWorldConfig>); };
            registration.validate          = [](const Any &config, std::string &error) {
                const auto *value = config.GetAsConst<ParticleWorldConfig>();
                return !value || ValidateParticleWorldConfig(*value, error);
            };
            return WorldSubSystemRegistry::Get().Register(Name(ParticleSystem::NAME.data()), std::move(registration));
        }
        void Shutdown() override
        {
            WorldSubSystemRegistry::Get().Unregister(Name(ParticleSystem::NAME.data()));
        }
    };
} // namespace sky::particle
REGISTER_MODULE_TAGGED(sky::particle::ParticleWorldModule)
