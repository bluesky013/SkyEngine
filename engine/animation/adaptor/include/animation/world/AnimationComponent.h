#pragma once

#include <animation/IAnimationInstanceControl.h>
#include <framework/world/Component.h>

namespace sky::animation {
    struct AnimationParameterOverride {
        Uuid  parameter;
        bool  isBoolean                                            = false;
        bool  booleanValue                                         = false;
        float floatValue                                           = 0.f;
        bool  operator==(const AnimationParameterOverride &) const = default;
    };
    struct AnimationComponentData {
        std::vector<AnimationParameterOverride> parameters;

        AnimationMode       mode = AnimationMode::Clip;
        Uuid                skeleton;
        Uuid                clip;
        Uuid                graph;
        bool                autoplay                                         = true;
        bool                loop                                             = true;
        float               playRate                                         = 1.f;
        AnimationRootPolicy rootPolicy                                       = AnimationRootPolicy::KeepInPose;
        bool                operator==(const AnimationComponentData &) const = default;
    };
    class AnimationComponent final : public ComponentAdaptor<AnimationComponentData> {
    public:
        COMPONENT_RUNTIME_INFO(AnimationComponent)
        void SetParameters(const std::vector<AnimationParameterOverride> &value)
        {
            data.parameters = value;
        }
        const std::vector<AnimationParameterOverride> &GetParameters() const
        {
            return data.parameters;
        }
        static void Reflect(SerializationContext *context);
        void        OnAttachToWorld() override
        {
            attachment = Uuid::Create();
        }
        void OnDetachFromWorld() override
        {
            attachment = {};
        }
        void SetConfig(const AnimationComponentData &value)
        {
            data = value;
        }
        const Uuid &GetAttachment() const
        {
            return attachment;
        }

        void SetSkeleton(const Uuid &value)
        {
            data.skeleton = value;
        }
        const Uuid &GetSkeleton() const
        {
            return data.skeleton;
        }
        void SetClip(const Uuid &value)
        {
            data.clip = value;
        }
        const Uuid &GetClip() const
        {
            return data.clip;
        }
        void SetGraph(const Uuid &value)
        {
            data.graph = value;
        }
        const Uuid &GetGraph() const
        {
            return data.graph;
        }
        void SetMode(AnimationMode value)
        {
            data.mode = value;
        }
        AnimationMode GetMode() const
        {
            return data.mode;
        }
        void SetAutoplay(bool value)
        {
            data.autoplay = value;
        }
        bool GetAutoplay() const
        {
            return data.autoplay;
        }
        void SetLoop(bool value)
        {
            data.loop = value;
        }
        bool GetLoop() const
        {
            return data.loop;
        }
        void SetPlayRate(float value)
        {
            data.playRate = value;
        }
        float GetPlayRate() const
        {
            return data.playRate;
        }
        void SetRootPolicy(AnimationRootPolicy value)
        {
            data.rootPolicy = value;
        }
        AnimationRootPolicy GetRootPolicy() const
        {
            return data.rootPolicy;
        }

    private:
        Uuid attachment;
    };
} // namespace sky::animation
