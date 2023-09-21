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
        primitive->instanceSet = geomFeature->RequestResourceGroup();
        primitive->instanceSet->BindDynamicUBO("localData", ubo, 0);
        primitive->instanceSet->Update();

        ResetPrimitive(geomFeature->GetDefaultTech());
        ResizeVertex(DEFAULT_VERTEX_CAPACITY);
    }

    void GeometryRenderer::Upload()
    {
        BuildVertexAssembly();
        uint8_t *ptr = vertexBuffer->GetRHIBuffer()->Map();
        memcpy(ptr, batchVertices.data(), batchVertices.size() * sizeof(GeometryBatchVertex));
        vertexBuffer->GetRHIBuffer()->UnMap();

        primitive->args = {args};
    }

    void GeometryRenderer::UpdateTransform(const Matrix4 &matrix)
    {
        ubo->Write(0, matrix);
        ubo->Write(sizeof(Matrix4), matrix.InverseTranspose());
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
        auto normal = (quad.v[0] - quad.v[1]).Cross((quad.v[2] - quad.v[1]));
        normal.Normalize();

        AddVertex({Cast(quad.v[0]), Cast(normal), currentColor});
        AddVertex({Cast(quad.v[1]), Cast(normal), currentColor});
        AddVertex({Cast(quad.v[2]), Cast(normal), currentColor});

        AddVertex({Cast(quad.v[2]), Cast(normal), currentColor});
        AddVertex({Cast(quad.v[3]), Cast(normal), currentColor});
        AddVertex({Cast(quad.v[0]), Cast(normal), currentColor});

        args.vertexCount += 6;
        return *this;
    }

    GeometryRenderer &GeometryRenderer::DrawAABB(const AABB &aabb)
    {
        //
        currentColor = Color(1.f, 0.f, 0.f, 1.f);
        AddVertex({Vector4{aabb.max.x, aabb.min.y, aabb.min.z, 1.0f}, Cast(-VEC3_Z), currentColor});
        AddVertex({Vector4{aabb.min.x, aabb.min.y, aabb.min.z, 1.0f}, Cast(-VEC3_Z), currentColor});
        AddVertex({Vector4{aabb.min.x, aabb.max.y, aabb.min.z, 1.0f}, Cast(-VEC3_Z), currentColor});
        AddVertex({Vector4{aabb.min.x, aabb.max.y, aabb.min.z, 1.0f}, Cast(-VEC3_Z), currentColor});
        AddVertex({Vector4{aabb.max.x, aabb.max.y, aabb.min.z, 1.0f}, Cast(-VEC3_Z), currentColor});
        AddVertex({Vector4{aabb.max.x, aabb.min.y, aabb.min.z, 1.0f}, Cast(-VEC3_Z), currentColor});

        AddVertex({Vector4{aabb.min.x, aabb.min.y, aabb.max.z, 1.0f}, Cast(VEC3_Z), currentColor});
        AddVertex({Vector4{aabb.max.x, aabb.min.y, aabb.max.z, 1.0f}, Cast(VEC3_Z), currentColor});
        AddVertex({Vector4{aabb.max.x, aabb.max.y, aabb.max.z, 1.0f}, Cast(VEC3_Z), currentColor});
        AddVertex({Vector4{aabb.max.x, aabb.max.y, aabb.max.z, 1.0f}, Cast(VEC3_Z), currentColor});
        AddVertex({Vector4{aabb.min.x, aabb.max.y, aabb.max.z, 1.0f}, Cast(VEC3_Z), currentColor});
        AddVertex({Vector4{aabb.min.x, aabb.min.y, aabb.max.z, 1.0f}, Cast(VEC3_Z), currentColor});

        //
        currentColor = Color(0.f, 1.f, 0.f, 1.f);
        AddVertex({Vector4{aabb.min.x, aabb.min.y, aabb.min.z, 1.0f}, Cast(-VEC3_X), currentColor});
        AddVertex({Vector4{aabb.min.x, aabb.min.y, aabb.max.z, 1.0f}, Cast(-VEC3_X), currentColor});
        AddVertex({Vector4{aabb.min.x, aabb.max.y, aabb.max.z, 1.0f}, Cast(-VEC3_X), currentColor});
        AddVertex({Vector4{aabb.min.x, aabb.max.y, aabb.max.z, 1.0f}, Cast(-VEC3_X), currentColor});
        AddVertex({Vector4{aabb.min.x, aabb.max.y, aabb.min.z, 1.0f}, Cast(-VEC3_X), currentColor});
        AddVertex({Vector4{aabb.min.x, aabb.min.y, aabb.min.z, 1.0f}, Cast(-VEC3_X), currentColor});

        AddVertex({Vector4{aabb.max.x, aabb.min.y, aabb.min.z, 1.0f}, Cast(VEC3_X), currentColor});
        AddVertex({Vector4{aabb.max.x, aabb.max.y, aabb.min.z, 1.0f}, Cast(VEC3_X), currentColor});
        AddVertex({Vector4{aabb.max.x, aabb.max.y, aabb.max.z, 1.0f}, Cast(VEC3_X), currentColor});
        AddVertex({Vector4{aabb.max.x, aabb.max.y, aabb.max.z, 1.0f}, Cast(VEC3_X), currentColor});
        AddVertex({Vector4{aabb.max.x, aabb.min.y, aabb.max.z, 1.0f}, Cast(VEC3_X), currentColor});
        AddVertex({Vector4{aabb.max.x, aabb.min.y, aabb.min.z, 1.0f}, Cast(VEC3_X), currentColor});

        //
        currentColor = Color(0.f, 0.f, 1.f, 1.f);
        AddVertex({Vector4{aabb.min.x, aabb.min.y, aabb.min.z, 1.0f}, Cast(-VEC3_Y), currentColor});
        AddVertex({Vector4{aabb.max.x, aabb.min.y, aabb.min.z, 1.0f}, Cast(-VEC3_Y), currentColor});
        AddVertex({Vector4{aabb.max.x, aabb.min.y, aabb.max.z, 1.0f}, Cast(-VEC3_Y), currentColor});
        AddVertex({Vector4{aabb.max.x, aabb.min.y, aabb.max.z, 1.0f}, Cast(-VEC3_Y), currentColor});
        AddVertex({Vector4{aabb.min.x, aabb.min.y, aabb.max.z, 1.0f}, Cast(-VEC3_Y), currentColor});
        AddVertex({Vector4{aabb.min.x, aabb.min.y, aabb.min.z, 1.0f}, Cast(-VEC3_Y), currentColor});

        AddVertex({Vector4{aabb.min.x, aabb.max.y, aabb.min.z, 1.0f}, Cast(VEC3_Y), currentColor});
        AddVertex({Vector4{aabb.min.x, aabb.max.y, aabb.max.z, 1.0f}, Cast(VEC3_Y), currentColor});
        AddVertex({Vector4{aabb.max.x, aabb.max.y, aabb.max.z, 1.0f}, Cast(VEC3_Y), currentColor});
        AddVertex({Vector4{aabb.max.x, aabb.max.y, aabb.max.z, 1.0f}, Cast(VEC3_Y), currentColor});
        AddVertex({Vector4{aabb.max.x, aabb.max.y, aabb.min.z, 1.0f}, Cast(VEC3_Y), currentColor});
        AddVertex({Vector4{aabb.min.x, aabb.max.y, aabb.min.z, 1.0f}, Cast(VEC3_Y), currentColor});

        args.vertexCount += 36;
        return *this;
    }

    GeometryRenderer &GeometryRenderer::Clear()
    {
        currentVertex = 0;
        args = rhi::CmdDrawLinear{};
        return *this;
    }

    void GeometryRenderer::AddVertex(const GeometryBatchVertex &vtx)
    {
        if (currentVertex >= vertexCapacity) {
            ResizeVertex(vertexCapacity * 2);
            needRebuildVA = true;
        }
        batchVertices[currentVertex] = vtx;
        batchVertices[currentVertex++].position += currentCenter;
    }

    void GeometryRenderer::BuildVertexAssembly()
    {
        if (needRebuildVA) {
            rhi::VertexAssembly::Descriptor desc = {};
            desc.vertexBuffers.emplace_back(vertexBuffer->GetRHIBuffer()->CreateView(rhi::BufferViewDesc{0, batchVertices.size() * sizeof(GeometryBatchVertex)}));
            primitive->va = RHI::Get()->GetDevice()->CreateVertexAssembly(desc);
            needRebuildVA = false;
        }
    }

} // namespace sky
