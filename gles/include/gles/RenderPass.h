//
// Created by Zach on 2023/2/1.
//

#pragma once

#include <rhi/RenderPass.h>
#include <gles/DevObject.h>

namespace sky::gles {

    class RenderPass : public rhi::RenderPass, public DevObject {
    public:
        RenderPass(Device &dev) : DevObject(dev) {}
        ~RenderPass() = default;

        bool Init(const Descriptor &desc);

    private:
        Descriptor descriptor;
    };

}
