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
#include <rhi/Device.h>

namespace sky {

    struct TechniqueInstance {
        RDGfxTechPtr             technique;
        RDMaterialInstancePtr    material;
        ShaderOptionPtr          shaderOption;

        // cache values
        RDProgramPtr             program;
        rhi::VertexInputPtr      vertexDesc;
        rhi::VertexAssemblyPtr   vao;
        rhi::GraphicsPipelinePtr pso;

        uint32_t                 vaoVersion     = 0;
        uint32_t                 variantHash    = 0;
        uint32_t                 renderPassHash = 0;
    };

    struct RenderPrimitive {
        explicit RenderPrimitive(PmrResource *res = nullptr) : args(res != nullptr ? res : &defaultRes) {}
        // dynamic status
        PmrUnSyncPoolRes defaultRes;
        uint32_t sortKey = 0;

        bool isReady     = false;

        AABB localBound {Vector3(std::numeric_limits<float>::min()), Vector3(std::numeric_limits<float>::max())};
        AABB worldBound {Vector3(std::numeric_limits<float>::min()), Vector3(std::numeric_limits<float>::max())};

        // geometry
        RenderVertexFlags   vertexFlags;
        RenderGeometryPtr   geometry;
        PmrVector<DrawArgs> args;
        rhi::BufferPtr      indirectBuffer;

        // shader resources
        RDResourceGroupPtr batchSet;
        RDResourceGroupPtr instanceSet;

        // cache object
        std::vector<TechniqueInstance> techniques;
    };

    struct RenderDrawItem {
        RenderPrimitive *primitive = nullptr;
        uint32_t techIndex = 0;
    };

} // namespace sky
