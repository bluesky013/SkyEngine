//
// Created by blues on 2025/2/3.
//

#pragma once

#include <render/renderpass/RasterPass.h>

namespace sky {
    class RenderScenePipeline;

    class DepthPass : public RasterPass {
    public:
        explicit DepthPass(rhi::PixelFormat ds, rhi::SampleCount samples_);
        ~DepthPass() override = default;

        void SetLayout(const RDResourceLayoutPtr &layout_);
    private:
        void SetupSubPass(rdg::RasterSubPassBuilder& builder, RenderScene &scene) override;
    };

    class DepthResolvePass : public FullScreenPass {
    public:
        explicit DepthResolvePass(const RDGfxTechPtr &tech, const Name& in, const Name& out);
        ~DepthResolvePass() override = default;

        void Setup(rdg::RenderGraph &rdg, RenderScene &scene) override;
    private:

        RDUniformBufferPtr ubo;
        RDResourceLayoutPtr layout;
    };

    class HizGenerateMip : public FullScreenPass {
    public:
        explicit HizGenerateMip(const RDGfxTechPtr &tech, const Name& in, const Name& out, const rhi::AccessFlags& final);
        ~HizGenerateMip() override = default;

        void SetInputSize(uint32_t width, uint32_t height);
        void Setup(rdg::RenderGraph &rdg, RenderScene &scene) override;
    private:
        RDUniformBufferPtr ubo;
        RDResourceLayoutPtr layout;

        uint32_t srcWidth = 1;
        uint32_t srcHeight = 1;

        Name outName;
        Name outUBOName;
        rhi::AccessFlags finalFlags;
    };

    class HizGenerator {
    public:
        HizGenerator(const RDGfxTechPtr &resolveTech, const RDGfxTechPtr &downTech);
        ~HizGenerator() = default;

        void BuildHizPass(rdg::RenderGraph &rdg, const rhi::ImagePtr& hiz, uint32_t width, uint32_t height);
        void AddPass(RenderScenePipeline& pipeline);
    private:
        RDGfxTechPtr technique;
        rhi::ImagePtr hizDepth;
        std::unique_ptr<HizGenerateMip> depthResolve;

        std::vector<std::unique_ptr<HizGenerateMip>> mips;
        std::vector<rhi::ImageViewPtr> mipViews;
        rhi::ImageViewPtr fullMipView;
    };

} // namespace sky
