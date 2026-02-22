//
// Created by blues on 2024/9/3.
//

#pragma once

#include <render/renderpass/PassBase.h>
#include <unordered_map>

namespace sky {

    namespace rdg {
        struct RasterSubPassBuilder;
    } // namespace rdg

    class RasterPass : public PassBase {
    public:
        explicit RasterPass(const Name &name_)
            : PassBase(name_)
        {}
        ~RasterPass() override = default;

        void Setup(rdg::RenderGraph &rdg, RenderScene &scene) override;
        void Resize(uint32_t w, uint32_t h);

    protected:
        virtual void SetupSubPass(rdg::RasterSubPassBuilder& builder, RenderScene &scene) {}

        struct Attachment {
            rdg::RasterAttachment attachment;
            rhi::ClearValue clearValue;
        };

        struct ComputeResource {
            Name name;
            rdg::ComputeView computeView;
        };

        struct SamplerResource {
            Name name;
            Name viewName;
        };

        uint32_t width  = 1;
        uint32_t height = 1;

        std::vector<ComputeResource> computeResources;
        std::vector<SamplerResource> samplers;

        std::vector<Attachment> colors;
        std::vector<Attachment> resolves;
        Attachment depthStencil;
        Attachment depthResolve;
    };

    class FullScreenPass : public RasterPass {
    public:
        explicit FullScreenPass(const Name &name_, const RDGfxTechPtr &tech) // NOLINT
            : RasterPass(name_)
            , technique(tech)
        {}
        ~FullScreenPass() override = default;

        void Setup(rdg::RenderGraph &rdg, RenderScene &scene) override;
    private:
        RDGfxTechPtr technique;
    };

} // namespace sky