//
// Created by Zach Lee on 2021/12/23.
//

#pragma once

#include <engine/render/service/RenderService.h>

namespace sky {

    class CameraService
        : public RenderService
        , public IComponentListener {
    public:
        CameraService();
        ~CameraService();

        void OnTick(float time) override;
    };

}