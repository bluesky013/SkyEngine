//
// Created by Zach Lee on 2021/12/23.
//

#pragma once

#include <engine/IService.h>

namespace sky {

    class CameraService
        : public IService
        , public IComponentListener
    {
    public:
        CameraService();
        ~CameraService();

        void OnTick(float time) override;
    };

}