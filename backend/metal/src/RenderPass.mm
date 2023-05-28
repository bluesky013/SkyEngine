//
// Created by Zach Lee on 2023/5/28.
//

#include <mtl/RenderPass.h>
#include <core/platform/Platform.h>
#include <mtl/Conversion.h>

namespace sky::mtl {

    bool RenderPass::Init(const Descriptor &desc)
    {
        InitInputMap(desc);
        colorAttachments.reserve(colors.size());
        resolveAttachments.reserve(resolves.size());
        SKY_ASSERT(resolves.empty() || colors.size() == resolves.size());

        for (auto &color : colors) {
            colorAttachments.emplace_back();
            auto &attachment = colorAttachments.back();

            attachment.format = FromRHI(desc.attachments[color].format);
            samplerCount = static_cast<uint32_t>(desc.attachments[color].sample);
        }

        for (auto &resolve : resolves) {
            resolveAttachments.emplace_back();
            auto &attachment = colorAttachments.back();

            attachment.format = FromRHI(desc.attachments[resolve].format);
            SKY_ASSERT(desc.attachments[resolve].sample == rhi::SampleCount::X1);
        }

        if (depthStencil != INVALID_INDEX) {
            auto &ds = desc.attachments[depthStencil];
            auto *formatInfo = rhi::GetImageInfoByFormat(ds.format);
            hasDepth = formatInfo->hasDepth;
            hasStencil = formatInfo->hasStencil;
            dsAttachment.format = FromRHI(ds.format);
            samplerCount = static_cast<uint32_t>(ds.sample);
        }

        return true;
    }

} // namespace sky::mtl
