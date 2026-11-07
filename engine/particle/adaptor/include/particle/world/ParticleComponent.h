#pragma once

#include <core/util/Uuid.h>
#include <framework/world/Component.h>

namespace sky::particle {

    struct ParticleComponentData {
        Uuid  effect;
        bool  autoplay = true;
        bool  loop     = true;
        bool     visible  = true;
        float    rateScale = 1.f;
        uint32_t budget    = 0;

        bool operator==(const ParticleComponentData &) const = default;
    };

    class ParticleComponent final : public ComponentAdaptor<ParticleComponentData> {
    public:
        COMPONENT_RUNTIME_INFO(ParticleComponent)
        static void Reflect(SerializationContext *context);

        void OnAttachToWorld() override
        {
            attachment = Uuid::Create();
        }
        void OnDetachFromWorld() override
        {
            attachment = {};
        }

        void SetEffect(const Uuid &value)
        {
            data.effect = value;
        }
        const Uuid &GetEffect() const
        {
            return data.effect;
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
        void SetVisible(bool value)
        {
            data.visible = value;
        }
        bool GetVisible() const
        {
            return data.visible;
        }
        void SetRateScale(float value)
        {
            data.rateScale = value;
        }
        float GetRateScale() const
        {
            return data.rateScale;
        }
        void SetBudget(uint32_t value)
        {
            data.budget = value;
        }
        uint32_t GetBudget() const
        {
            return data.budget;
        }
        const Uuid &GetAttachment() const
        {
            return attachment;
        }

    private:
        Uuid attachment;
    };

} // namespace sky::particle
