//
// Created by blues on 2026/2/19.
//

#pragma once

#include <render/resource/Mesh.h>
#include <render/RenderPrimitive.h>

namespace sky {

    struct RenderBatch {
        explicit  RenderBatch(const RDGfxTechPtr& tech) : techInst(tech) {}

        void Update(const ShaderVariantKey& pipelineKey) noexcept;
        bool UpdateBatchGroupLayout() noexcept;
        void UpdateBatchOptions() noexcept;
        void UpdateShadingBuffer() noexcept;
        void UpdateBatchGroup() noexcept;

        bool IsReadyToGather() const noexcept;
        void BuildPipelineState(const RenderBatchPrepareInfo& info) noexcept;
        void Prepare(const RenderBatchPrepareInfo& info) noexcept;
        uint32_t CalculatePipelineHash(uint32_t passHash) const noexcept;

        const Name& GetRasterID() const noexcept;

        RenderTechniqueInstance    techInst;
        rhi::VertexAssemblyPtr     vao;
        rhi::GraphicsPipelinePtr   pso;
        RDDynamicUniformBufferPtr  shadingBuffer;
        RDResourceLayoutPtr        layout;
        RDResourceGroupPtr         batchGroup;

        // weak reference
        const MaterialInstance* material = nullptr;
        const RenderGeometry*   geometry = nullptr;
        DynamicUniformBuffer*   boneBuffer = nullptr;

        // cache status
        uint32_t batchLayoutHash = ~(0U);
        uint32_t valueVersion = ~(0U);
        uint32_t batchVersion = ~(0U);
        uint32_t psoKey = ~(0U);
    };

    struct RenderSection {
        bool PrepareBatch(const RenderBatchPrepareInfo& info) noexcept;

        DrawArgs args;
        std::vector<std::unique_ptr<RenderBatch>> batches;
    };

    struct RenderMaterialPrimitive : RenderPrimitive {
        explicit RenderMaterialPrimitive(const RDMeshPtr& mesh);

        void SetBoneData(const std::vector<RDDynamicUniformBufferPtr> &boneData) noexcept;
        void SetVertexFlags(const RenderVertexFlags& flags) noexcept;

        bool IsReady() const noexcept override;
        void UpdateWorldBounds(const Matrix4& localToWorld) noexcept override;
        bool PrepareBatch(const RenderBatchPrepareInfo& info) noexcept override;
        void GatherRenderItem(IRenderItemGatherContext* context) noexcept override;

        std::vector<RDMaterialInstancePtr> materials;
        std::vector<RenderSection> sections;

        // shader resources
        RDResourceGroupPtr instanceSet;
    };

} // namespace sky
