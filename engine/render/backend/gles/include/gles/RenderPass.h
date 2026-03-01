//
// Created by Zach on 2023/2/1.
//

#pragma once

#include <vector>
#include <rhi/RenderPass.h>
#include <gles/DevObject.h>

namespace sky::gles {

    class RenderPass : public rhi::RenderPass, public DevObject {
    public:
        RenderPass(Device &dev) : DevObject(dev) {}

        ~RenderPass() = default;

        bool Init(const Descriptor &desc);

        const std::vector<Attachment> &GetAttachments() const { return attachments; };
        const std::vector<SubPass> &GetSubPasses() const { return subPasses; }

        const std::vector<uint32_t> &GetGLColors() const { return colors; }
        const std::vector<uint32_t> &GetGLResolves() const { return resolves; }
        uint32_t GetGLDepthStencil() const { return depthStencil; }
        uint32_t GetGLDSResolve() const { return dsResolve; }

    private:
        std::vector<Attachment> attachments;
        std::vector<SubPass> subPasses;
    };

    using RenderPassPtr = std::shared_ptr<RenderPass>;
}
