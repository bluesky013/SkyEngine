#pragma once

#include <animation/assets/AnimationComponentData.h>
#include <framework/world/Component.h>

namespace sky::animation {
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
        static void            Reflect(SerializationContext *context);
        AnimationStatus        TryLoadJson(JsonInputArchive &archive);
        void                   LoadJson(JsonInputArchive &archive) override;
        bool                   TryAttachToWorld(std::string &error) override;
        const AnimationStatus &GetLoadStatus() const
        {
            return loadStatus;
        }
        void OnAttachToWorld() override
        {
            attachment = Uuid::Create();
        }
        void OnDetachFromWorld() override
        {
            attachment = {};
        }
        void SetConfig(const AnimationComponentData &value)
        {
            data       = value;
            loadStatus = {};
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
        bool SetMode(AnimationMode value)
        {
            if (value < AnimationMode::Clip || value >= AnimationMode::END) {
                return false;
            }
            data.mode = value;
            return true;
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
        Uuid            attachment;
        AnimationStatus loadStatus;
    };
} // namespace sky::animation
