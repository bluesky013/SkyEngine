//
// Created by Zach Lee on 2022/5/28.
//

#pragma once

#include <render/resources/RenderResource.h>
#include <render/resources/Shader.h>
#include <render/resources/Pass.h>
#include <vulkan/GraphicsPipeline.h>
#include <vulkan/DescriptorSetBinder.h>

namespace sky {

    class GraphicsTechnique : public RenderResource {
    public:
        GraphicsTechnique() = default;
        ~GraphicsTechnique() = default;

        void SetShaderTable(RDGfxShaderTablePtr table);

        void SetRenderPass(RDPassPtr pass, uint32_t subPass = 0);

        drv::GraphicsPipelinePtr AcquirePso(drv::VertexInputPtr& vertexInput);

        drv::GraphicsPipelinePtr AcquirePso(drv::VertexInputPtr& vi, drv::ShaderOptionPtr option);

        void SetViewTag(uint32_t tag);

        void SetDrawTag(uint32_t tag);

        uint32_t GetViewTag() const;

        uint32_t GetDrawTag() const;

        RDGfxShaderTablePtr GetShaderTable() const;

        drv::DescriptorSetBinderPtr CreateSetBinder() const;

    private:
        bool CheckVertexInput(drv::VertexInput& input) const;

        RDGfxShaderTablePtr table;
        uint32_t subPassIndex = 0;
        uint32_t viewTag = 0;
        uint32_t drawTag = 0;
        RDPassPtr pass;
        drv::GraphicsPipeline::State pipelineState;
        std::unordered_map<uint32_t, drv::GraphicsPipelinePtr> psoCache;
    };
    using RDGfxTechniquePtr = std::shared_ptr<GraphicsTechnique>;

}