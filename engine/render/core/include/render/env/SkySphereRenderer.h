//
// Created by blues on 2023/12/18.
//

#pragma once

#include <render/resource/Buffer.h>
#include <render/resource/Mesh.h>
#include <render/mesh/StandardMeshDefines.h>
#include <render/RenderPrimitive.h>

namespace sky {
    class RenderScene;

    struct SkyBoxVertex {
        Vector4 pos;
        Vector2 uv;
    };

    struct SkySpherePrimitive : RenderPrimitive {
        explicit SkySpherePrimitive(const RDGfxTechPtr &inTech) : techInst(inTech)
        {
            shouldUseFrustumCulling = false;
        }

        void UpdateTexture(const RDTexture2DPtr &tex);
        bool IsReady() const noexcept override;
        bool PrepareBatch(const RenderBatchPrepareInfo &info) noexcept override;
        void GatherRenderItem(IRenderItemGatherContext *context) noexcept override;

        RenderTechniqueInstance   techInst;
        
        rhi::DescriptorSetPoolPtr pool;
        rhi::GraphicsPipelinePtr  pso;
        rhi::CmdDrawIndexed       arg;
       
        RDTexture2DPtr     texture;
        RDResourceGroupPtr batchGroup;
    };

    class SkySphereRenderer {
    public:
        SkySphereRenderer(RenderScene *scene, const RDGfxTechPtr &tech);
        ~SkySphereRenderer();

        void UpdateTexture(const RDTexture2DPtr &texture);
    private:
        void BuildSphere();

        RenderScene *renderScene;
        RDGfxTechPtr technique;

        rhi::DescriptorSetPoolPtr pool;
        float radius = 500000.f;
        std::unique_ptr<SkySpherePrimitive> primitive;
    };

} // namespace sky