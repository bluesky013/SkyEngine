#include <animation/world/AnimationComponent.h>
#include <framework/serialization/PropertyCommon.h>
#include <framework/world/ComponentFactory.h>

namespace sky::animation {
    AnimationStatus AnimationComponent::TryLoadJson(JsonInputArchive &archive)
    {
        bool validMode = true;
        archive.ForEachMember([&](const std::string &key, const JsonInputArchive::Value &value) {
            if (key != "elements" || !value.IsObject()) {
                return;
            }
            const auto mode = value.FindMember("mode");
            if (mode != value.MemberEnd()) {
                validMode = mode->value.IsUint64() && mode->value.GetUint64() < static_cast<uint64_t>(AnimationMode::END);
            }
        });
        if (!validMode) {
            loadStatus = {AnimationError::InvalidAsset, "Animation mode is outside the supported range"};
            return loadStatus;
        }
        AnimationComponentData next;
        archive.LoadValueObject(next);
        data       = std::move(next);
        loadStatus = {};
        return loadStatus;
    }

    void AnimationComponent::LoadJson(JsonInputArchive &archive)
    {
        TryLoadJson(archive);
    }

    bool AnimationComponent::TryAttachToWorld(std::string &error)
    {
        if (!loadStatus) {
            error = loadStatus.message;
            return false;
        }
        OnAttachToWorld();
        return true;
    }

    void AnimationComponent::Reflect(SerializationContext *context)
    {
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
        ComponentFactory::Get()->RegisterComponent<AnimationComponent>("Animation", {Name("Animation")});
    }
} // namespace sky::animation
