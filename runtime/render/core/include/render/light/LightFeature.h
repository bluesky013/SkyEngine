//
// Created by blues on 2024/12/8.
//

#pragma once

#include <core/environment/Singleton.h>

namespace sky {

    class LightFeature : public Singleton<LightFeature> {
    public:
        LightFeature() = default;
        ~LightFeature() override = default;

        void Init();
    };

} // namespace sky