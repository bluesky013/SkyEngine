//
// Created by Zach Lee on 2022/7/28.
//

#include <render/RenderCamera.h>

namespace sky {

    void RenderCamera::Init()
    {
        renderView = std::make_shared<RenderView>();
    }

}