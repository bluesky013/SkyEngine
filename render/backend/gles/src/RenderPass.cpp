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
        return true;
    }
}
