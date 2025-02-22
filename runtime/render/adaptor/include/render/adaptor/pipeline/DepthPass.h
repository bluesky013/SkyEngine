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
        explicit HizGenerateMip(const RDGfxTechPtr &tech, const Name& in, const Name& out);
        ~HizGenerateMip() override = default;
    private:
        RDResourceLayoutPtr layout;
    };

    class HizGenerator {
    public:
        HizGenerator(const RDGfxTechPtr &resolveTech, const RDGfxTechPtr &downTech);
        ~HizGenerator() = default;

        void BuildHizPass(rdg::RenderGraph &rdg, uint32_t width, uint32_t height);
        void AddPass(RenderScenePipeline& pipeline);
    private:
        RDGfxTechPtr technique;
        std::unique_ptr<DepthResolvePass> depthResolve;
        std::vector<std::unique_ptr<HizGenerateMip>> mips;
    };

} // namespace sky
