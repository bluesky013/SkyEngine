//
// Created by Zach Lee on 2023/8/19.
//

#pragma once

#include <core/shapes/AABB.h>
#include <core/std/Container.h>
#include <render/RenderDrawArgs.h>
#include <render/RenderGeometry.h>
#include <render/resource/Material.h>
#include <render/resource/ResourceGroup.h>
#include <render/resource/Technique.h>
#include <shader/ShaderVariant.h>
#include <rhi/Device.h>

namespace sky {
    struct RenderBatch {
        void SetOption(const Name &name, uint8_t val)
        {
            technique->SetOption(name, val, batchKey);
        }

        RDGfxTechPtr      technique;

        // override states
        rhi::PrimitiveTopology   topo = rhi::PrimitiveTopology::TRIANGLE_LIST;

        RDProgramPtr              program;
        rhi::VertexAssemblyPtr    vao;
        rhi::GraphicsPipelinePtr  pso;
        rhi::VertexInputPtr       vertexDesc;

        RDResourceGroupPtr        batchGroup;

        // cache status
        uint32_t vaoVersion      = 0;
        uint32_t renderPassHash  = 0;
        uint32_t batchLayoutHash = 0;
        ShaderVariantKey batchKey;
        ShaderVariantKey cacheFinalKey;

        uint32_t valueVersion = ~(0U);
        uint32_t batchVersion = ~(0U);
    };

    struct RenderPrimitive {
        // before program ready
        virtual void PrepareBatch() {}

        // update resource group, program ready
        virtual void UpdateBatch() {}

        virtual bool IsReady() const { return true; }

        uint32_t sortKey = 0;

        AABB localBound {Vector3(std::numeric_limits<float>::min()), Vector3(std::numeric_limits<float>::max())};
        AABB worldBound {Vector3(std::numeric_limits<float>::min()), Vector3(std::numeric_limits<float>::max())};

        // geometry
        RenderVertexFlags     vertexFlags;
        RenderGeometryPtr     geometry;
        MeshletGeometryPtr    meshlets;

        bool clusterValid = false;

        std::vector<DrawArgs> args;
        rhi::BufferPtr        indirectBuffer;

        // shader resources
        RDResourceGroupPtr instanceSet;

        // cache object
        uint32_t vaoVersion = ~(0U);
        std::vector<RenderBatch> batches;
    };

    struct RenderMaterialPrimitive : public RenderPrimitive {
        void PrepareBatch() override;
        void UpdateBatch() override;
        bool IsReady() const override;

        RDMaterialInstancePtr     material;
        std::vector<RDDynamicUniformBufferPtr> batchBuffers;
    };

    struct RenderDrawItem {
        RenderPrimitive *primitive = nullptr;
        uint32_t techIndex = 0;
    };

} // namespace sky
