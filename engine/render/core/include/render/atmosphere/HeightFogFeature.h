//
// Created by SkyEngine on 2025/3/7.
//

#pragma once

#include <render/FeatureProcessor.h>
#include <render/RenderBuiltinLayout.h>
#include <render/resource/Buffer.h>
#include <core/environment/Singleton.h>

namespace sky {

    class HeightFogFeature : public Singleton<HeightFogFeature> {
    public:
        HeightFogFeature() = default;
        ~HeightFogFeature() override = default;

        void Init();
    };

    class HeightFogFeatureProcessor : public IFeatureProcessor {
    public:
        explicit HeightFogFeatureProcessor(RenderScene *scn);
        ~HeightFogFeatureProcessor() override = default;

        void SetFogColor(const Vector4 &color);
        void SetInscatterColor(const Vector4 &color);
        void SetFogDensity(float density);
        void SetHeightFalloff(float falloff);
        void SetBaseHeight(float height);
        void SetMaxHeight(float height);
        void SetStartDistance(float distance);

        const HeightFogParams &GetParams() const { return params; }

        void Tick(float time) override {}
        void Render(rdg::RenderGraph &rdg) override;

    private:
        HeightFogParams    params;
        RDUniformBufferPtr ubo;
    };

} // namespace sky
