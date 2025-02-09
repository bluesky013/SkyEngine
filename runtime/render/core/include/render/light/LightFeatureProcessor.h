//
// Created by blues on 2024/6/10.
//

#pragma once

#include <render/FeatureProcessor.h>
#include <render/light/LightBase.h>
#include <render/resource/Buffer.h>
#include <memory>

namespace sky {

    class LightFeatureProcessor : public IFeatureProcessor {
    public:
        explicit LightFeatureProcessor(RenderScene *scn) : IFeatureProcessor(scn) {}
        ~LightFeatureProcessor() override = default;

        void Tick(float time) override;
        void Render(rdg::RenderGraph &rdg) override {}

        template <typename T>
        T* CreateLight()
        {
            static_assert(std::is_base_of_v<Light, T>);
            auto light = new T();
            AddLight(light);
            return light;
        }

        MainDirectLight* GetOrCreateMainLight();
        MainDirectLight* GetMainLight() const { return mainLight.get(); }
        void RemoveMainLight();

        void AddLight(Light *light);
        void RemoveLight(Light *light);
    private:
        void GatherLightInfo();

        using LightPtr = std::unique_ptr<Light>;

        std::unique_ptr<MainDirectLight> mainLight;
        std::vector<LightPtr> lights;

        RDBufferPtr lightData;
        RDBufferPtr stagingBuffer;
    };

} // namespace sky
