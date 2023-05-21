//
// Created by Zach on 2023/2/1.
//

#include <gles/RenderPass.h>
#include <gles/Conversion.h>
#include <gles/Device.h>

namespace sky::gles {

    bool RenderPass::Init(const Descriptor &desc)
    {
        InitInputMap(desc);
        attachments = desc.attachments;
        subPasses = desc.subPasses;

        attachmentGLInfos.resize(attachments.size());

        for (uint32_t i = 0; i < subPasses.size(); ++i) {
            auto &sub = subPasses[i];
            if (sub.depthStencil.index != INVALID_INDEX) {
                auto &attachment = attachments[sub.depthStencil.index];
                auto &attachmentInfo = attachmentGLInfos[sub.depthStencil.index];
                const auto &feature = GetFormatFeature(attachment.format);
                attachmentInfo.hasDepth = HasDepth(attachment.format);
                attachmentInfo.hasStencil = HasStencil(attachment.format);
                depthStencil = sub.depthStencil.index;
            }
        }
        return true;
    }

}
