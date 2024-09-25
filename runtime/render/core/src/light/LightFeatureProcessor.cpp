//
// Created by blues on 2024/6/10.
//

#include <render/light/LightFeatureProcessor.h>

namespace sky {

    void LightFeatureProcessor::Render(rdg::RenderGraph &rdg)
    {

    }

    void LightFeatureProcessor::AddLight(Light *light)
    {
        lights.emplace_back(light);
    }

    void LightFeatureProcessor::RemoveLight(Light *light)
    {
        lights.erase(std::remove_if(lights.begin(), lights.end(), [light](const LightPtr &v) {
            return light == v.get();
        }), lights.end());
    }

} // namespace sky