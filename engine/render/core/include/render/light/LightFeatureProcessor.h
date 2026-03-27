//
// Created by blues on 2024/6/10.
//

#pragma once

#include <render/FeatureProcessor.h>
#include <render/light/LightBase.h>
#include <render/resource/Buffer.h>
#include <render/RenderBuiltinLayout.h>
#include <memory>

namespace sky {

    struct TimeOfDay {
        void Tick(float time);
        void UpdateTime(float time);

        float current = 0.f;
        Vector3 position;
        float radius = 200.f;
        MainDirectLight *light = nullptr;
    };

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

        uint32_t GetPointLightCount() const { return pointLightCount; }
        uint32_t GetSpotLightCount() const { return spotLightCount; }
        const ShaderPointLightBuffer &GetPointLightBuffer() const { return pointLightBuffer; }
        const ShaderSpotLightBuffer &GetSpotLightBuffer() const { return spotLightBuffer; }

        void GatherPunctualLightData();

    private:
        void GatherLightInfo();

        using LightPtr = std::unique_ptr<Light>;

        std::unique_ptr<MainDirectLight> mainLight;
        std::vector<LightPtr> lights;

        RDBufferPtr lightData;
        RDBufferPtr stagingBuffer;

        uint32_t pointLightCount = 0;
        uint32_t spotLightCount  = 0;
        ShaderPointLightBuffer pointLightBuffer = {};
        ShaderSpotLightBuffer spotLightBuffer = {};

        TimeOfDay tod;
    };

} // namespace sky
