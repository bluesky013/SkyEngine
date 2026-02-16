//
// Created by Copilot on 2026/2/16.
//

#pragma once

#include <core/environment/Singleton.h>

namespace sky {

    class HLODFeature : public Singleton<HLODFeature> {
    public:
        HLODFeature() = default;
        ~HLODFeature() override = default;

        void Init();
    };

} // namespace sky
