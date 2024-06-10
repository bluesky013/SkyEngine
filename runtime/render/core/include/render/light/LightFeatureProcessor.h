//
// Created by blues on 2024/6/10.
//

#pragma once

#include <render/FeatureProcessor.h>
#include <render/light/LightBase.h>
#include <memory>

namespace sky {

    class LightFeatureProcessor : public IFeatureProcessor {
    public:
        explicit  LightFeatureProcessor(RenderScene *scn) : IFeatureProcessor(scn) {}
        ~LightFeatureProcessor() override = default;

        void Render(rdg::RenderGraph &rdg) override;

        template <typename T>
        T* CreateLight()
        {
            static_assert(std::is_base_of_v<Light, T>);
            auto light = new T();
            AddLight(light);
            return light;
        }

        void AddLight(Light *light);
        void RemoveLight(Light *light);
    private:
        using LightPtr = std::unique_ptr<Light>;

        std::vector<LightPtr> lights;
    };

} // namespace sky
