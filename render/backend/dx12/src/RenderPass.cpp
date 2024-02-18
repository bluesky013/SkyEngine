//
// Created by blues on 2024/2/11.
//

#include <dx12/RenderPass.h>
#include <core/platform/Platform.h>
#include <dx12/Conversion.h>

namespace sky::dx {

    bool RenderPass::Init(const Descriptor &desc)
    {
        SKY_ASSERT(desc.subPasses.size() == 1);
        const auto &pass = desc.subPasses[0];

        colorFormats.resize(pass.colors.size());
        for (size_t i = 0; i < pass.colors.size(); ++i) {
            colorFormats[i] = FromRHI(desc.attachments[pass.colors[i].index].format);
        }
        depthStencilFormat = FromRHI(desc.attachments[pass.depthStencil.index].format);
        return true;
    }

} // sky::dx