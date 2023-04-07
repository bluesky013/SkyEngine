//
// Created by Zach Lee on 2023/4/7.
//

#include <scene_render/Technique.h>
#include <rhi/Device.h>
#include <IRHI.h>

namespace sky::rhi {

    void GraphicsTechnique::BuildPso()
    {
        pso = Interface<IRHI>::Get()->GetApi()->GetDevice()->CreateGraphicsPipeline(psoDesc);
    }

}