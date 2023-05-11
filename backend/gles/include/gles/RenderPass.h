//
// Created by Zach on 2023/2/1.
//

#pragma once

#include <vector>
#include <rhi/RenderPass.h>
#include <gles/DevObject.h>

namespace sky::gles {

    using DrawBuffer = std::vector<GLenum>;

    class RenderPass : public rhi::RenderPass, public DevObject {
    public:
        RenderPass(Device &dev) : DevObject(dev) {}

        ~RenderPass() = default;

        struct AttachmentGLInfo {
            uint32_t index = INVALID_INDEX; // index to color buffers
            bool hasDepth = false;
            bool hasStencil = false;
        };

        bool Init(const Descriptor &desc);

        const std::vector<Attachment> &GetAttachments() const { return attachments; };
        const std::vector<AttachmentGLInfo> &GetAttachmentGLInfos() const { return attachmentGLInfos; };
        const std::vector<SubPass> &GetSubPasses() const { return subPasses; }
        const DrawBuffer &GetDrawBuffer(uint32_t index) const { return drawBuffers[index]; }

        const std::vector<uint32_t> &GetGLColors() const { return colors; }
        const std::vector<uint32_t> &GetGLResolves() const { return resolves; }
        uint32_t GetGLDepthStencil() const
        {
            return depthStencil;
        }

    private:
        std::vector<Attachment> attachments;
        std::vector<AttachmentGLInfo> attachmentGLInfos;
        std::vector<SubPass> subPasses;

        std::vector<uint32_t> colors;
        std::vector<uint32_t> resolves;
        std::vector<uint32_t> inputs;
        uint32_t depthStencil = INVALID_INDEX;
        std::vector<DrawBuffer> drawBuffers;
    };

    using RenderPassPtr = std::shared_ptr<RenderPass>;
}
