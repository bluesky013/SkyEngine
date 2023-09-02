//
// Created by Zach Lee on 2023/9/2.
//

#pragma once

#include <SampleScene.h>

namespace sky {

    class SampleSceneCube : public SampleScene {
    public:
        SampleSceneCube() = default;
        ~SampleSceneCube() override = default;

        void Tick(float time) override;
    };

} // namespace sky