//
// Created by blues on 2025/2/28.
//

#include <render/adaptor/pipeline/EmptyPass.h>

namespace sky {

    EmptyPass::EmptyPass() : RasterPass(Name("empty"))
    {
        Name swapChainName(SWAP_CHAIN.data());

        colors.emplace_back(Attachment{
            rdg::RasterAttachment{swapChainName, rhi::LoadOp::CLEAR, rhi::StoreOp::STORE},
            rhi::ClearValue(0.2f, 0.2f, 0.2f, 0)
        });
    }

} // namespace sky