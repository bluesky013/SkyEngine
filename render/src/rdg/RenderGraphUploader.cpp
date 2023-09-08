//
// Created by Zach Lee on 2023/9/8.
//

#include <render/rdg/RenderGraphUploader.h>
#include <render/rdg/RenderGraph.h>

namespace sky::rdg {

    void RenderGraphUploader::UploadConstantBuffers()
    {
        auto encoder = rdg.context->MainCommandBuffer()->EncodeBlit();
        auto &constantBuffers = rdg.resourceGraph.constantBuffers;
        for (auto &cb : constantBuffers) {
            if (cb.ubo) {
                cb.ubo->Upload(*encoder);
            }
        }
    }

} // namespace sky::rdg
