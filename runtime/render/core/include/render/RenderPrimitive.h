//
// Created by Zach Lee on 2023/8/19.
//

#pragma once

#include <core/shapes/AABB.h>
#include <core/std/Container.h>
#include <render/RenderDrawArgs.h>
#include <render/resource/Material.h>
#include <render/resource/ResourceGroup.h>
#include <render/resource/Technique.h>
#include <rhi/Device.h>

namespace sky {

    struct TechniqueInstance {
        RDGfxTechPtr technique;
        rhi::RenderPassPtr renderPass;
        rhi::GraphicsPipelinePtr pso;
        std::vector<std::string> processors;
        uint32_t setMask = 0;
        bool rebuildPso = true;
    };

    struct RenderPrimitive {
        explicit RenderPrimitive(PmrResource *res = nullptr) : args(res != nullptr ? res : &defaultRes) {}
        std::vector<TechniqueInstance> techniques;

        PmrUnSyncPoolRes defaultRes;
        uint32_t sortKey = 0;
        AABB boundingBox {Vector3(std::numeric_limits<float>::min()), Vector3(std::numeric_limits<float>::max())};

        RDResourceGroupPtr batchSet;
        RDResourceGroupPtr instanceSet;

        rhi::VertexAssemblyPtr va;
        rhi::BufferPtr indirectBuffer;
        PmrVector<DrawArgs> args;
    };

    struct RenderDrawItem {
        RenderPrimitive *primitive = nullptr;
        uint32_t techIndex = 0;
    };

} // namespace sky
