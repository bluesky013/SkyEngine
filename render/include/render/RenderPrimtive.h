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
        drv::VertexInputPtr vertexInput;
        drv::GraphicsPipelinePtr pso;
        drv::CmdDraw args = {};
        uint32_t drawTag = 0;
    };
    using RDGfxTechniqueProxyPtr = std::unique_ptr<GraphicsTechniqueProxy>;


    class RenderPrimitive {
    public:
        RenderPrimitive() = default;
        virtual ~RenderPrimitive() = default;

        SKY_DISABLE_COPY(RenderPrimitive)

        void SetMesh(RDMeshPtr& value, uint32_t subMesh = 0);

        inline void SetObjectSet(RDDesGroupPtr set)
        {
            objSet = set;
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

        const std::vector<RDGfxTechniqueProxyPtr>& GetTechniques() const
        {
            return graphicTechniques;
        }

        RDDesGroupPtr GetMaterialSet() const
        {
            return matSet;
        }

    protected:
        uint32_t subMeshIndex = 0;
        uint32_t viewMask = 0;
        RDDesGroupPtr objSet;
        RDDesGroupPtr matSet;
        RDMeshPtr mesh;
        drv::CmdDraw args {};
        drv::VertexAssemblyPtr vertexAssembly;
        std::vector<RDGfxTechniqueProxyPtr> graphicTechniques;
    };
}