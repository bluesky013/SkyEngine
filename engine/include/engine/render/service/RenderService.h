//
// Created by Zach Lee on 2022/2/1.
//

#pragma once

#include <engine/IService.h>

namespace sky {

    class RenderScene;

    class RenderService : public IService {
    public:
        RenderService() = default;
        ~RenderService() = default;

        virtual void Render(RenderScene&) {}
    };

}