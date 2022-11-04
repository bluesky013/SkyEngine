//
// Created by Zach Lee on 2022/8/1.
//

#pragma once

#include <core/util/Macros.h>
#include <render/RenderPrimitive.h>
#include <render/resources/DescirptorGroup.h>
#include <vulkan/DescriptorSetBinder.h>
#include <vulkan/DrawItem.h>
#include <vulkan/VertexAssembly.h>

namespace sky {
    class Mesh;

    struct GraphicsTechniqueProxy {
        vk::DescriptorSetBinderPtr setBinder;
        vk::VertexAssemblyPtr      assembly;
        vk::VertexInputPtr         vertexInput;
        vk::GraphicsPipelinePtr    pso;
        vk::CmdDraw                args    = {};
        uint32_t                    drawTag = 0;
    };
    using RDGfxTechniqueProxyPtr = std::unique_ptr<GraphicsTechniqueProxy>;

    class RenderMeshPrimitive : public RenderPrimitive {
    public:
        RenderMeshPrimitive()          = default;
        virtual ~RenderMeshPrimitive() = default;

        SKY_DISABLE_COPY(RenderMeshPrimitive)

        void SetMesh(Mesh &value, uint32_t subMesh = 0);

        inline void SetVertexAssembly(vk::VertexAssemblyPtr value)
        {
            vertexAssembly = value;
        }

        const std::vector<RDGfxTechniqueProxyPtr> &GetTechniques() const
        {
            return graphicTechniques;
        }

        RDDesGroupPtr GetMaterialSet() const
        {
            return matSet;
        }

        void Encode(RenderRasterEncoder *) override;

    protected:
        uint32_t                            subMeshIndex = 0;
        RDDesGroupPtr                       matSet;
        vk::CmdDraw                        args{};
        vk::VertexAssemblyPtr              vertexAssembly;
        std::vector<RDGfxTechniqueProxyPtr> graphicTechniques;
    };
} // namespace sky
