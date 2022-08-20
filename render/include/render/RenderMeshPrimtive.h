//
// Created by Zach Lee on 2022/8/1.
//

#pragma once

#include <vulkan/DescriptorSetBinder.h>
#include <vulkan/VertexAssembly.h>
#include <vulkan/DrawItem.h>
#include <core/util/Macros.h>
#include <render/resources/DescirptorGroup.h>
#include <render/RenderPrimitive.h>

namespace sky {
    class Mesh;

    struct GraphicsTechniqueProxy {
        drv::DescriptorSetBinderPtr setBinder;
        drv::VertexAssemblyPtr assembly;
        drv::VertexInputPtr vertexInput;
        drv::GraphicsPipelinePtr pso;
        drv::CmdDraw args = {};
        uint32_t drawTag = 0;
    };
    using RDGfxTechniqueProxyPtr = std::unique_ptr<GraphicsTechniqueProxy>;


    class RenderMeshPrimitive : public RenderPrimitive {
    public:
        RenderMeshPrimitive() = default;
        virtual ~RenderMeshPrimitive() = default;

        SKY_DISABLE_COPY(RenderMeshPrimitive)

        void SetMesh(Mesh& value, uint32_t subMesh = 0);

        inline void SetVertexAssembly(drv::VertexAssemblyPtr value)
        {
            vertexAssembly = value;
        }

        const std::vector<RDGfxTechniqueProxyPtr>& GetTechniques() const
        {
            return graphicTechniques;
        }

        RDDesGroupPtr GetMaterialSet() const
        {
            return matSet;
        }

        void Encode(RenderRasterEncoder*) override;

    protected:
        uint32_t subMeshIndex = 0;
        RDDesGroupPtr matSet;
        drv::CmdDraw args {};
        drv::VertexAssemblyPtr vertexAssembly;
        std::vector<RDGfxTechniqueProxyPtr> graphicTechniques;
    };
}