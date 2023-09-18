//
// Created by blues on 2023/9/18.
//

#pragma once

#include <SampleScene.h>
#include <framework/world/World.h>
#include <framework/world/GameObject.h>
#include <render/resource/Texture.h>

namespace sky {

    class SampleRayMarching : public SampleScene {
    public:
        SampleRayMarching() = default;
        ~SampleRayMarching() override = default;

        bool Start(RenderWindow *window) override;
        void Shutdown() override;
        void Resize(uint32_t width, uint32_t height) override;

    private:
        GameObject *meshObj = nullptr;
        GameObject *camera = nullptr;
    };

} // namespace sky
