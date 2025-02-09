//
// Created by blues on 2025/2/3.
//

#include <render/adaptor/pipeline/DepthPass.h>

namespace sky {

    DepthPass::DepthPass(uint32_t width, uint32_t height)
        : RasterPass(Name("DepthPass"))
    {

    }

    void DepthPass::SetupSubPass(rdg::RasterSubPassBuilder& builder, RenderScene &scene)
    {

    }

} // namespace sky