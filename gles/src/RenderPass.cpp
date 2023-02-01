//
// Created by Zach on 2023/2/1.
//

#include <gles/RenderPass.h>

namespace sky::gles {

    bool RenderPass::Init(const Descriptor &desc)
    {
        descriptor = desc;
        return true;
    }

}
