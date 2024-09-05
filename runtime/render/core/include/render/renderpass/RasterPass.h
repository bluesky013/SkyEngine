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

        uint32_t width;
        uint32_t height;

        std::vector<ComputeResource> computeResources;

        std::vector<Attachment> colors;
        std::vector<Attachment> resolves;
        Attachment depthStencil;
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