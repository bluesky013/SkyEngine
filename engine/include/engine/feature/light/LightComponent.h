//
// Created by Zach Lee on 2021/11/13.
//

#pragma once

#include <engine/world/Component.h>

namespace sky {

    class LightComponent : public Component {
    public:
        LightComponent()  = default;
        ~LightComponent() = default;

        TYPE_RTTI_WITH_VT(LightComponent)

        static void Reflect();
    };

} // namespace sky