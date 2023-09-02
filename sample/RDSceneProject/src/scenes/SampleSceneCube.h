//
// Created by Zach Lee on 2023/9/2.
//

#pragma once

#include <SampleScene.h>
#include <framework/world/World.h>
#include <framework/world/GameObject.h>
namespace sky {

    class SampleSceneCube : public SampleScene {
    public:
        SampleSceneCube() = default;
        ~SampleSceneCube() override = default;

        bool Start(RenderWindow *window) override;
        void Resize(uint32_t width, uint32_t height) override;

    private:
        GameObject *cube = nullptr;
        GameObject *camera = nullptr;
    };

} // namespace sky
