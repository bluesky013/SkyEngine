#include <animation/world/AnimationComponent.h>
#include <framework/serialization/PropertyCommon.h>
#include <framework/world/ComponentFactory.h>

namespace sky::animation {
    void AnimationComponent::Reflect(SerializationContext *context)
    {
        context->Register<AnimationMode>("AnimationMode").Enum(AnimationMode::Clip, "Clip").Enum(AnimationMode::Graph, "Graph");
        context->Register<AnimationRootPolicy>("AnimationRootPolicy")
            .Enum(AnimationRootPolicy::KeepInPose, "KeepInPose")
            .Enum(AnimationRootPolicy::LockToReference, "LockToReference");
        context->Register<AnimationParameterOverride>("AnimationParameterOverride")
            .Member<&AnimationParameterOverride::parameter>("parameter")
            .Member<&AnimationParameterOverride::isBoolean>("isBoolean")
            .Member<&AnimationParameterOverride::booleanValue>("booleanValue")
            .Member<&AnimationParameterOverride::floatValue>("floatValue");
        context->Register<AnimationComponentData>("AnimationComponentData")
            .Member<&AnimationComponentData::parameters>("parameters")
            .Member<&AnimationComponentData::mode>("mode")
            .Member<&AnimationComponentData::skeleton>("skeleton")
            .Member<&AnimationComponentData::clip>("clip")
            .Member<&AnimationComponentData::graph>("graph")
            .Member<&AnimationComponentData::autoplay>("autoplay")
            .Member<&AnimationComponentData::loop>("loop")
            .Member<&AnimationComponentData::playRate>("playRate")
            .Member<&AnimationComponentData::rootPolicy>("rootPolicy");
        context->Register<AnimationComponent>("AnimationComponent")
            .Member<&AnimationComponent::SetParameters, &AnimationComponent::GetParameters>("parameters")
            .Member<&AnimationComponent::SetMode, &AnimationComponent::GetMode>("mode")
            .Member<&AnimationComponent::SetSkeleton, &AnimationComponent::GetSkeleton>("skeleton")
            .Property(static_cast<uint32_t>(CommonPropertyKey::ASSET_TYPE), Any(std::string("AuroraSkeleton")))
            .Member<&AnimationComponent::SetClip, &AnimationComponent::GetClip>("clip")
            .Property(static_cast<uint32_t>(CommonPropertyKey::ASSET_TYPE), Any(std::string("AnimationClip")))
            .Member<&AnimationComponent::SetGraph, &AnimationComponent::GetGraph>("graph")
            .Property(static_cast<uint32_t>(CommonPropertyKey::ASSET_TYPE), Any(std::string("AnimationGraph")))
            .Member<&AnimationComponent::SetAutoplay, &AnimationComponent::GetAutoplay>("autoplay")
            .Member<&AnimationComponent::SetLoop, &AnimationComponent::GetLoop>("loop")
            .Member<&AnimationComponent::SetPlayRate, &AnimationComponent::GetPlayRate>("playRate")
            .Property(static_cast<uint32_t>(CommonPropertyKey::RANGE_MIN), Any(0.f))
            .Member<&AnimationComponent::SetRootPolicy, &AnimationComponent::GetRootPolicy>("rootPolicy");
        ComponentFactory::Get()->RegisterComponent<AnimationComponent>("Animation");
    }
} // namespace sky::animation
