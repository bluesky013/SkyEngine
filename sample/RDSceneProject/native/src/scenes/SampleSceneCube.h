//
// Created by Zach Lee on 2023/9/2.
//

#pragma once

#include "framework/world/Actor.h"
#include "framework/world/World.h"
#include <SampleScene.h>
#include <render/resource/Texture.h>

namespace sky {

    class SampleSceneCube : public SampleScene {
    public:
        SampleSceneCube() = default;
        ~SampleSceneCube() override = default;

        bool Start(RenderWindow *window) override;
        void Shutdown() override;
        void Resize(uint32_t width, uint32_t height) override;

    private:
        Actor *cube = nullptr;
        Actor *camera = nullptr;
    };

} // namespace sky
