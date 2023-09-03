//
// Created by Zach Lee on 2023/8/31.
//

#include <render/resource/Technique.h>

namespace sky {

    void Technique::AddShader(const sky::RDShaderPtr &shader)
    {
        shaders.emplace_back(shader);
    }

    void GraphicsTechnique::SetDepthStencil(const rhi::DepthStencil &ds)
    {
        state.depthStencil = ds;
    }

    void GraphicsTechnique::SetRasterState(const rhi::RasterState &rs)
    {
        state.rasterState = rs;
    }

    void GraphicsTechnique::SetBlendState(const std::vector<rhi::BlendState> &blends)
    {
        state.blendStates = blends;
    }
} // namespace sky