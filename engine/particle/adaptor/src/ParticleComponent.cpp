#include <framework/serialization/PropertyCommon.h>
#include <framework/world/ComponentFactory.h>
#include <particle/world/ParticleComponent.h>
#include <particle/world/ParticleSystem.h>

namespace sky::particle {
    void ParticleComponent::Reflect(SerializationContext *context)
    {
        context->Register<ParticleComponentData>("ParticleComponentData")
            .Member<&ParticleComponentData::effect>("effect")
            .Member<&ParticleComponentData::autoplay>("autoplay")
            .Member<&ParticleComponentData::loop>("loop")
            .Member<&ParticleComponentData::visible>("visible")
            .Member<&ParticleComponentData::rateScale>("rateScale")
            .Member<&ParticleComponentData::budget>("budget");
        context->Register<ParticleComponent>("ParticleComponent")
            .Member<&ParticleComponent::SetEffect, &ParticleComponent::GetEffect>("effect")
            .Property(static_cast<uint32_t>(CommonPropertyKey::ASSET_TYPE), Any(std::string("ParticleEffect")))
            .Member<&ParticleComponent::SetAutoplay, &ParticleComponent::GetAutoplay>("autoplay")
            .Member<&ParticleComponent::SetLoop, &ParticleComponent::GetLoop>("loop")
            .Member<&ParticleComponent::SetVisible, &ParticleComponent::GetVisible>("visible")
            .Member<&ParticleComponent::SetRateScale, &ParticleComponent::GetRateScale>("rateScale")
            .Property(static_cast<uint32_t>(CommonPropertyKey::RANGE_MIN), Any(0.f))
            .Member<&ParticleComponent::SetBudget, &ParticleComponent::GetBudget>("budget");
        ComponentFactory::Get()->RegisterComponent<ParticleComponent>("Particle", {Name(ParticleSystem::NAME.data())});
    }
} // namespace sky::particle
