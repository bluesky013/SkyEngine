//
// Created by Zach Lee on 2023/8/19.
//

#pragma once

#include <core/shapes/AABB.h>
#include <render/RenderDrawArgs.h>
#include <render/RenderGeometry.h>
#include <render/resource/Material.h>
#include <render/resource/ResourceGroup.h>
#include <render/resource/Technique.h>
#include <shader/ShaderVariant.h>
#include <render/lod/LodGroup.h>
#include <rhi/Device.h>

namespace sky {
    class SceneView;

    struct RenderBatchPrepareInfo {
        Name techId;
        ShaderVariantKey pipelineKey;
        rhi::RenderPassPtr pass;
        uint32_t subPassId = 0;
    };

    struct RenderTechniqueInstance {
        explicit RenderTechniqueInstance(const RDGfxTechPtr& inTech) : technique(inTech) {}

        void SetOption(const Name &name, uint8_t val)
        {
            technique->SetOption(name, val, batchKey);
        }

        void SetVertexFlags(const RenderVertexFlags &flags)
        {
            technique->ProcessVertexVariantKey(flags, vertexKey);
        }

        bool UpdateProgram(const ShaderVariantKey& pipelineKey);

        RDGfxTechPtr     technique;
        RDProgramPtr     program;

        ShaderVariantKey batchKey;
        ShaderVariantKey vertexKey;
        ShaderVariantKey cacheFinalKey{ShaderVariantKeyInit::INVALID};
    };

    struct RenderItem {
        RenderGeometryPtr  geometry;

        RDResourceGroupPtr batchGroup;
        RDResourceGroupPtr instanceSet;

        rhi::GraphicsPipelinePtr pso;
        rhi::VertexAssemblyPtr   vao;

        std::vector<DrawArgs> args;
    };

    struct IRenderItemGatherContext {
        IRenderItemGatherContext() = default;
        virtual ~IRenderItemGatherContext() = default;

        virtual void Append(RenderItem&& item) = 0;

        Name rasterID;
        SceneView* sceneView;
    };

    struct RenderPrimitive {
        virtual ~RenderPrimitive() = default;

        // before program ready
        virtual void Prepare(const SceneView* view) noexcept {}

        virtual bool PrepareBatch(const RenderBatchPrepareInfo& info) noexcept { return false; }

        virtual void UpdateWorldBounds(const Matrix4& localToWorld) noexcept {}

        virtual void GatherRenderItem(IRenderItemGatherContext* context) noexcept {}

        virtual bool IsReady() const noexcept { return true; }

        uint32_t sortKey = 0;
        bool shouldUseFrustumCulling = true;

        BoundingBoxSphere worldBounds;

        // geometry
        RenderGeometryPtr geometry;
    };

} // namespace sky
