//
// Created by blues on 2024/9/6.
//

#pragma once

#include <render/renderpass/RasterPass.h>
#include <render/adaptor/pipeline/DefaultPassConstants.h>

namespace sky {

    class PresentPass : public PassBase {
    public:
        explicit PresentPass(const rhi::SwapChainPtr &swc);
        ~PresentPass() override = default;

        void Prepare(rdg::RenderGraph &rdg, RenderScene &scene) override;
        void Setup(rdg::RenderGraph &rdg, RenderScene &scene) override;

    private:
        rhi::SwapChainPtr swapChain;
    };

} // namespace sky
