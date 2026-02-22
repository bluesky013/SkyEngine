//
// Created by blues on 2024/9/3.
//

#include <render/renderpass/PassBase.h>
#include <render/rdg/RenderGraph.h>
#include <render/RenderScene.h>

namespace sky {

    void PassBase::Prepare(rdg::RenderGraph &rdg, RenderScene &scene)
    {
        auto &rg = rdg.resourceGraph;

        for (auto &[key, image] : images) {
            rg.AddImage(key, image);
        }

        for (auto &[key, buffer] : buffers) {
            rg.AddBuffer(key, buffer);
        }
    }

} // namespace sky