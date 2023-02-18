//
// Created by Zach on 2023/2/1.
//

#include <gles/RenderPass.h>

namespace sky::gles {

    bool RenderPass::Init(const Descriptor &desc)
    {
        attachments = desc.attachments;
        subPasses = desc.subPasses;
        return true;
    }

}
