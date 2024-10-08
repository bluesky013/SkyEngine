//
// Created by Zach Lee on 2022/11/13.
//

#pragma once

#include <vector>
#include <unordered_map>
#include <rhi/Core.h>

namespace sky::rhi {

    class RenderPass {
    public:
        RenderPass()          = default;
        virtual ~RenderPass() = default;

        struct Attachment {
            PixelFormat format       = PixelFormat::UNDEFINED;
            SampleCount sample       = SampleCount::X1;
            LoadOp      load         = LoadOp::DONT_CARE;
            StoreOp     store        = StoreOp::DONT_CARE;
            LoadOp      stencilLoad  = LoadOp::DONT_CARE;
            StoreOp     stencilStore = StoreOp::DONT_CARE;
        };

        struct AttachmentRef {
            uint32_t index = ~(0U);
            AccessFlags access = AccessFlagBit::COLOR_WRITE;
            AspectFlags mask = AspectFlagBit::COLOR_BIT;
        };

        struct SubPass {
            std::vector<AttachmentRef> colors;
            std::vector<AttachmentRef> resolves;
            std::vector<AttachmentRef> inputs;

            std::vector<uint32_t>      preserves;
            AttachmentRef              depthStencil = {~(0U), AccessFlagBit::DEPTH_STENCIL_WRITE, AspectFlagBit::DEPTH_BIT | AspectFlagBit::STENCIL_BIT};
            AttachmentRef              dsResolve    = {~(0U), AccessFlagBit::DEPTH_STENCIL_WRITE, AspectFlagBit::DEPTH_BIT | AspectFlagBit::STENCIL_BIT};
            uint32_t                   viewMask     = 0;
        };

        struct Dependency {
            uint32_t src = (~0U);
            uint32_t dst = (~0U);
            AccessFlags srcAccess;
            AccessFlags dstAccess;
        };

        struct Descriptor {
            std::vector<Attachment> attachments;
            std::vector<SubPass>    subPasses;
            std::vector<Dependency> dependencies;
            std::vector<uint32_t>   correlatedViewMasks;
        };
        using BindingMap = std::vector<uint32_t>;
        const BindingMap &GetInputMap(uint32_t subpass) const { return subpassInputMaps[subpass]; }
        const BindingMap &GetOutputMap(uint32_t subpass) const { return subpassOutputMaps[subpass]; }
        const std::vector<uint32_t> &GetAttachmentColorMap() const { return attachmentMap; }
        const std::vector<uint32_t> &GetColors() const { return colors; }
        const std::vector<uint32_t> &GetResolves() const { return resolves; }
        uint32_t GetDepthStencil() const { return depthStencil; }
        uint32_t GetDepthStencilResolve() const { return dsResolve; }
        uint32_t GetSubPassNum() const { return subPassNum; }
        rhi::SampleCount GetSamplerCount() const { return samplerCount; }

        uint32_t GetCompatibleHash() const { return compatibleHash; }

    protected:
        void InitInputMap(const Descriptor &desc);
        void CalculateHash(const Descriptor &desc);

        std::vector<BindingMap> subpassInputMaps;
        std::vector<BindingMap> subpassOutputMaps;

        std::vector<uint32_t> attachmentMap; // index of color & resolve
        std::vector<uint32_t> colors;
        std::vector<uint32_t> resolves;
        uint32_t depthStencil = INVALID_INDEX;
        uint32_t dsResolve    = INVALID_INDEX;
        uint32_t subPassNum = 1;
        rhi::SampleCount samplerCount = rhi::SampleCount::X1;

        uint32_t compatibleHash = 0;
        uint32_t hash = 0;
    };

    using RenderPassPtr = std::shared_ptr<RenderPass>;
};
