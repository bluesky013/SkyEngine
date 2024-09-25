//
// Created by blues on 2024/9/6.
//

#include <render/adaptor/pipeline/PresentPass.h>
#include <render/adaptor/pipeline/DefaultPassConstants.h>
#include <render/rdg/RenderGraph.h>

namespace sky {

    PresentPass::PresentPass(const rhi::SwapChainPtr &swc) // NOLINT
        : PassBase("Present")
        , swapChain(swc)
    {
    }

    void PresentPass::Prepare(rdg::RenderGraph &rdg, RenderScene &scene)
    {
        rdg.resourceGraph.ImportSwapChain(SWAP_CHAIN.data(), swapChain);
    }

    void PresentPass::Setup(rdg::RenderGraph &rdg, RenderScene &scene)
    {
        rdg.AddPresentPass(name.c_str(), SWAP_CHAIN.data());
    }

} // namespace sky