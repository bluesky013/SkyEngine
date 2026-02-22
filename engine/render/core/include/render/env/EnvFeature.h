//
// Created by blues on 2025/2/17.
//

#pragma once

#include <render/FeatureProcessor.h>
#include <render/resource/Texture.h>
#include <core/environment/Singleton.h>

namespace sky {

    class EnvFeature : public Singleton<EnvFeature> {
    public:
        EnvFeature() = default;
        ~EnvFeature() override = default;

        void Init();
    };

    class EnvFeatureProcessor : public IFeatureProcessor {
    public:
        explicit EnvFeatureProcessor(RenderScene *scn);
        ~EnvFeatureProcessor() override = default;

        void SetRadiance(const RDTextureCubePtr &tex);
        void SetIrradiance(const RDTextureCubePtr &tex);

        const RDTextureCubePtr &GetRadiance() const { return radiance; }
        const RDTextureCubePtr &GetIrradiance() const { return irradiance; }

        void Tick(float time) override {}
        void Render(rdg::RenderGraph &rdg) override;

    private:
        RDTextureCubePtr radiance;
        RDTextureCubePtr irradiance;
    };

} // namespace sky
