//
// Created by Zach Lee on 2021/12/23.
//

#pragma once

#include <engine/ISystem.h>

namespace sky {

    class CameraSystem : public ISystem {
    public:
        CameraSystem() = default;
        ~CameraSystem() = default;

        void OnTick(float time) override {}
    };

}