//
// Created by blues on 2024/6/10.
//

#include <render/light/LightBase.h>

namespace sky {

    void PointLight::Collect(LightInfo &info)
    {
        info.color = color;

    }

    void SpotLight::Collect(LightInfo &info)
    {
        info.color = color;
        info.position  = ToVec4(position);
        info.direction = ToVec4(direction);
    }

    void DirectLight::Collect(LightInfo &info)
    {
        info.color = color;
        info.direction = ToVec4(direction);
    }

} // namespace sky