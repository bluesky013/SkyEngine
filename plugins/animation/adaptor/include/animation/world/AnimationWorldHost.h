#pragma once

#include <animation/IAnimationInstanceControl.h>
#include <aurora/renderer/ISkeletalPoseSource.h>
#include <framework/world/World.h>

namespace sky::animation {
    inline constexpr const char *ANIMATION_WORLD_SYSTEM = "Animation";
    class AnimationWorldHost : public IAnimationInstanceControl, public aurora::ISkeletalPoseSource {
    public:
        Uuid             GetComponentType() const override;
        std::string_view GetWorldSystemName() const override
        {
            return ANIMATION_WORLD_SYSTEM;
        }
        virtual std::unique_ptr<IWorldSubSystem> CreateSystem() = 0;
        virtual void                             Drain()        = 0;
    };
    std::unique_ptr<AnimationWorldHost> CreateAnimationWorldHost(std::shared_ptr<const void> moduleLease);
} // namespace sky::animation
