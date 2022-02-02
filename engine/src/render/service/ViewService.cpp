//
// Created by Zach Lee on 2021/12/23.
//

#include <engine/render/service/ViewService.h>
#include <engine/render/camera/CameraComponent.h>

namespace sky {

    ViewService::ViewService()
    {
    }

    ViewService::~ViewService()
    {
    }

    void ViewService::OnTick(float time)
    {
        if (pool) {
            pool->SwapBuffer();
        }
    }
}