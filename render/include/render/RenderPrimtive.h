//
// Created by Zach Lee on 2022/8/1.
//

#pragma once

#include <render/resources/Mesh.h>
#include <vulkan/DescriptorSetBinder.h>
#include <vulkan/VertexAssembly.h>
#include <vulkan/DrawItem.h>
#include <core/util/Macros.h>

namespace sky {


    struct GraphicsTechniqueProxy {
        RDGfxTechniquePtr gfxTechnique;
        drv::DescriptorSetBinderPtr setBinder;
        drv::VertexAssemblyPtr assembly;
        drv::CmdDraw* args = nullptr;
        uint32_t drawTag = 0;
    };
    using RDGfxTechniqueProxyPtr = std::unique_ptr<GraphicsTechniqueProxy>;


    class RenderPrimitive {
    public:
        RenderPrimitive() = default;
        virtual ~RenderPrimitive() = default;

        SKY_DISABLE_COPY(RenderPrimitive)

        void SetMaterial(RDMaterialPtr value);

        inline void SetDrawArgs(const drv::CmdDraw& args)
        {
            drawArgs = args;
        }

        inline void SetAABB(const Box& value)
        {
            aabb = value;
        }

        inline void SetVertexAssembly(drv::VertexAssemblyPtr value)
        {
            vertexAssembly = value;
        }

        inline void SetViewTag(uint32_t tag)
        {
            viewMask |= tag;
        }

        inline uint32_t GetViewMask() const
        {
            return viewMask;
        }

    protected:
        Box aabb {};
        drv::CmdDraw drawArgs {};
        uint32_t viewMask = 0;
        RDMaterialPtr material;
        drv::VertexAssemblyPtr vertexAssembly;
        std::vector<RDGfxTechniqueProxyPtr> graphicTechniques;
    };
}