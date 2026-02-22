//
// Created by blues on 2024/9/6.
//

#include <render/adaptor/pipeline/PresentPass.h>
#include <render/adaptor/pipeline/DefaultPassConstants.h>
#include <render/rdg/RenderGraph.h>

namespace sky {

    PresentPass::PresentPass(const rhi::SwapChainPtr &swc) // NOLINT
        : PassBase(Name("Present"))
        , swapChain(swc)
        , swapchainName("SwapChain")
    {
    }

    void PresentPass::Prepare(rdg::RenderGraph &rdg, RenderScene &scene)
    {
        rdg.resourceGraph.ImportSwapChain(swapchainName, swapChain);
    }

    void PresentPass::Setup(rdg::RenderGraph &rdg, RenderScene &scene)
    {
        rdg.AddPresentPass(name, swapchainName);
    }

} // namespace sky