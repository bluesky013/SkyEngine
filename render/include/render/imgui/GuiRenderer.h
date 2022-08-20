//
// Created by Zach Lee on 2022/8/19.
//

#pragma once

#include <render/resources/Shader.h>
#include <render/resources/Buffer.h>
#include <render/resources/Technique.h>
#include <render/resources/Texture.h>
#include <render/resources/DescirptorGroup.h>
#include <render/RenderFeature.h>
#include <vulkan/VertexAssembly.h>
#include <vulkan/DescriptorSetBinder.h>

namespace sky {

    class GuiRenderer : public RenderFeature {
    public:
        GuiRenderer(RenderScene& scn) : RenderFeature(scn) {}
        ~GuiRenderer() = default;

        void Init();

        void OnTick(float time);

        void GatherRenderPrimitives();

        void OnRender();

    private:
        void CheckBufferSize(uint64_t vertexSize, uint64_t indexSize);

        RDGfxShaderTablePtr shaders;
        RDGfxTechniquePtr technique;
        RDBufferPtr vertexBuffer;
        RDBufferPtr indexBuffer;
        RDDesGroupPtr set;
        drv::DescriptorSetBinderPtr setBinder;
        drv::VertexAssemblyPtr assembly;
        std::unique_ptr<DescriptorPool> pool;
        uint64_t currentVertexSize = 0;
        uint64_t currentIndexSize = 0;
    };
}