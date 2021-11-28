//
// Created by Zach Lee on 2021/11/28.
//


#include <render/RenderScene.h>

namespace sky {

    void RenderScene::SetTarget(drv::SwapChain& swc)
    {
        swapChain = &swc;
    }

}