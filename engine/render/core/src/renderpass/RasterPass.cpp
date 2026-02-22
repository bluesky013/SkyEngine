//
// Created by blues on 2024/9/3.
//

#include <render/renderpass/RasterPass.h>
#include <render/rdg/RenderGraph.h>

namespace sky {

    void RasterPass::Resize(uint32_t w, uint32_t h)
    {
        width  = w;
        height = h;

        for (auto &image : images) {
            image.second.extent.width  = width;
            image.second.extent.height = height;
        }
    }

    void RasterPass::Setup(rdg::RenderGraph &rdg, RenderScene &scene)
    {
        auto passBuilder = rdg.AddRasterPass(name, width, height);

        auto colorCount   = static_cast<uint32_t>(colors.size());
        auto resolveCount = static_cast<uint32_t>(resolves.size());

        if (resolveCount != 0) {
            SKY_ASSERT(colorCount == resolveCount);
        }

        for (auto &color : colors) {
            passBuilder.AddAttachment(color.attachment, color.clearValue);
        }

        for (auto &resolve : resolves) {
            passBuilder.AddAttachment(resolve.attachment, resolve.clearValue);
        }

        if (!depthStencil.attachment.name.Empty()) {
            passBuilder.AddAttachment(depthStencil.attachment, depthStencil.clearValue);
        }

        auto subPassBuilder = passBuilder.AddRasterSubPass(Name("Sub0"));
        for (uint32_t i = 0; i < colorCount; ++i) {
            subPassBuilder.AddColor(colors[i].attachment.name, rdg::ResourceAccessBit::WRITE);

            if (resolveCount != 0) {
                subPassBuilder.AddResolve(resolves[i].attachment.name, rdg::ResourceAccessBit::WRITE);
            }
        }

        if (!depthStencil.attachment.name.Empty()) {
            subPassBuilder.AddDepthStencil(depthStencil.attachment.name, rdg::ResourceAccessBit::WRITE);
        }

        SetupSubPass(subPassBuilder, scene);

        for (auto &res : computeResources) {
            subPassBuilder.AddComputeView(res.name, res.computeView);
        }

        for (auto &res : samplers) {
            subPassBuilder.AddSamplerView(res.name, res.viewName);
        }
    }

    void FullScreenPass::Setup(rdg::RenderGraph &rdg, RenderScene &scene)
    {
        auto passBuilder = rdg.AddRasterPass(name, width, height);

        SKY_ASSERT(colors.size() == 1);
        passBuilder.AddAttachment(colors[0].attachment, colors[0].clearValue);

        auto subPassBuilder = passBuilder.AddRasterSubPass(Name("Sub0"));
        subPassBuilder.AddColor(colors[0].attachment.name, rdg::ResourceAccessBit::WRITE);

        for (auto &res : computeResources) {
            subPassBuilder.AddComputeView(res.name, res.computeView);
        }

        for (auto &res : samplers) {
            subPassBuilder.AddSamplerView(res.name, res.viewName);
        }

        subPassBuilder.AddFullScreen(Name("FullScreen"))
            .SetTechnique(technique);

        SetupSubPass(subPassBuilder, scene);
    }

} // namespace sky