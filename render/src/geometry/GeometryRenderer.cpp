//
// Created by Zach Lee on 2023/9/2.
//

#include <render/geometry/GeometryRenderer.h>

#include <core/math/MathUtil.h>

#include <render/geometry/GeometryFeature.h>
#include <render/RenderBuiltinLayout.h>
#include <render/Renderer.h>
#include <render/RHI.h>
#include <rhi/Queue.h>

namespace sky {
    static constexpr uint32_t DEFAULT_VERTEX_CAPACITY = 32;

    void GeometryRenderer::Init()
    {
        ubo = std::make_shared<DynamicUniformBuffer>();
        ubo->Init(sizeof(InstanceLocal), Renderer::Get()->GetInflightFrameCount());
        ubo->Write(0, InstanceLocal{Matrix4::Identity(), Matrix4::Identity()});
        ubo->Upload();

        auto *geomFeature = GeometryFeature::Get();
        primitive = std::make_unique<RenderPrimitive>();
        primitive->techniques.resize(1);
        primitive->techniques[0].vertexDesc = geomFeature->GetVertexDesc();
        primitive->instanceSet = geomFeature->RequestSet();
        primitive->instanceSet->BindBuffer(0, ubo->GetRHIBuffer(), 0, sizeof(InstanceLocal), 0);
        primitive->instanceSet->Update();

        ResetPrimitive(geomFeature->GetDefaultTech());
        ResizeVertex(DEFAULT_VERTEX_CAPACITY);
    }

    void GeometryRenderer::Upload()
    {
        uint8_t *ptr = vertexBuffer->GetRHIBuffer()->Map();
        memcpy(ptr, batchVertices.data(), batchVertices.size() * sizeof(GeometryBatchVertex));
        vertexBuffer->GetRHIBuffer()->UnMap();

        primitive->args = args;
    }

    void GeometryRenderer::UpdateTransform(const Matrix4 &matrix)
    {
        ubo->Write(0, matrix);
        ubo->Upload();
    }

    void GeometryRenderer::ResetPrimitive(const RDGfxTechPtr &tech)
    {
        primitive->techniques[0].technique = tech;
    }

    void GeometryRenderer::ResizeVertex(uint32_t size)
    {
        batchVertices.resize(size);
        vertexCapacity = size;
        vertexBuffer = std::make_shared<Buffer>();
        vertexBuffer->Init(batchVertices.size() * sizeof(GeometryBatchVertex),
                           rhi::BufferUsageFlagBit::VERTEX | rhi::BufferUsageFlagBit::TRANSFER_DST,
                           rhi::MemoryType::CPU_TO_GPU);

        rhi::VertexAssembly::Descriptor desc = {};
        desc.vertexBuffers.emplace_back(vertexBuffer->GetRHIBuffer()->CreateView(rhi::BufferViewDesc{0, size}));
        primitive->va = RHI::Get()->GetDevice()->CreateVertexAssembly(desc);
    }

    GeometryRenderer &GeometryRenderer::SetTechnique(const RDGfxTechPtr &tech)
    {
        technique = tech;
        ResetPrimitive(tech);
        return *this;
    }

    GeometryRenderer &GeometryRenderer::SetColor(const Color &color)
    {
        return *this;
    }

    GeometryRenderer &GeometryRenderer::DrawLine(const Line &)
    {
        return *this;
    }

    GeometryRenderer &GeometryRenderer::DrawTriangle(const Triangle &triangle)
    {
        return *this;
    }

    GeometryRenderer &GeometryRenderer::DrawQuad(const Quad &quad)
    {
        AddVertex({Cast(quad.v[0]), currentColor});
        AddVertex({Cast(quad.v[1]), currentColor});
        AddVertex({Cast(quad.v[2]), currentColor});
        AddVertex({Cast(quad.v[2]), currentColor});
        AddVertex({Cast(quad.v[3]), currentColor});
        AddVertex({Cast(quad.v[0]), currentColor});
        args.vertexCount = 6;
        return *this;
    }

    GeometryRenderer &GeometryRenderer::Clear()
    {
        currentVertex = 0;
        args.firstVertex = 0;
        args.vertexCount = 0;
        return *this;
    }

    void GeometryRenderer::AddVertex(const GeometryBatchVertex &vtx)
    {
        if (currentVertex >= vertexCapacity) {
            ResizeVertex(vertexCapacity * 2);
        }
        batchVertices[currentVertex++] = vtx;
    }

} // namespace sky
