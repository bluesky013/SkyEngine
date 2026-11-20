//
// Created by SkyEngine on 2025/3/7.
//

#pragma once

#include <core/math/Color.h>
#include <framework/world/Component.h>
#include <render/atmosphere/HeightFogFeature.h>

namespace sky {

    struct HeightFogData {
        ColorRGB fogColor        = ColorRGB{0.7f, 0.8f, 0.9f};
        ColorRGB inscatterColor  = ColorRGB{0.9f, 0.85f, 0.7f};
        float    fogDensity      = 0.02f;
        float    heightFalloff   = 0.15f;
        float    baseHeight      = 0.f;
        float    maxHeight       = 50.f;
        float    startDistance   = 0.f;
    };

    class HeightFogComponent : public ComponentAdaptor<HeightFogData> {
    public:
        HeightFogComponent() = default;
        ~HeightFogComponent() override = default;

        COMPONENT_RUNTIME_INFO(HeightFogComponent)

        static void Reflect(SerializationContext *context);

        void Tick(float time) override {}
        void OnAttachToWorld() override;
        void OnDetachFromWorld() override;

        void SetFogColor(const ColorRGB &color);
        const ColorRGB &GetFogColor() const { return data.fogColor; }

        void SetInscatterColor(const ColorRGB &color);
        const ColorRGB &GetInscatterColor() const { return data.inscatterColor; }

        void SetFogDensity(float density);
        float GetFogDensity() const { return data.fogDensity; }

        void SetHeightFalloff(float falloff);
        float GetHeightFalloff() const { return data.heightFalloff; }

        void SetBaseHeight(float height);
        float GetBaseHeight() const { return data.baseHeight; }

        void SetMaxHeight(float height);
        float GetMaxHeight() const { return data.maxHeight; }

        void SetStartDistance(float distance);
        float GetStartDistance() const { return data.startDistance; }

    private:
        HeightFogFeatureProcessor *fp = nullptr;
    };

} // namespace sky
