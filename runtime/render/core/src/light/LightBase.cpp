//
// Created by blues on 2024/6/10.
//

#include <render/light/LightBase.h>

namespace sky {

    void DirectLight::Collect(LightInfo &info)
    {
        info.direction = direction;
        info.color = color;
    }

} // namespace sky