//
// Created by Zach Lee on 2023/9/16.
//

#pragma once

#include "framework/world/GameObject.h"
#include "framework/world/World.h"
#include <SampleScene.h>
#include <render/resource/Texture.h>

namespace sky {
    class SampleMesh : public SampleScene {
    public:
        SampleMesh() = default;
        ~SampleMesh() override = default;

        bool Start(RenderWindow *window) override;
        void Shutdown() override;
        void Resize(uint32_t width, uint32_t height) override;

    private:
        GameObject *meshObj = nullptr;
        GameObject *camera = nullptr;
    };

} // namespace sky