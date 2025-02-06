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

    struct GeometryPrimitive : public RenderPrimitive {
    };

    void GeometryRenderer::Init()
    {
        ubo = new DynamicUniformBuffer();
        ubo->Init(sizeof(InstanceLocal));
        ubo->WriteT(0, InstanceLocal{Matrix4::Identity(), Matrix4::Identity()});

        auto *geomFeature = GeometryFeature::Get();
        primitive = std::make_unique<GeometryPrimitive>();
        primitive->batches.resize(1);
        primitive->instanceSet = geomFeature->RequestResourceGroup();
        primitive->instanceSet->BindDynamicUBO(Name("Local"), ubo, 0);
        primitive->instanceSet->Update();

        ResetPrimitive(geomFeature->GetDefaultTech());
        ResizeVertex(DEFAULT_VERTEX_CAPACITY);
    }

    void GeometryRenderer::Upload()
    {
        uint8_t *ptr = vertexBuffer->GetRHIBuffer()->Map();
        memcpy(ptr, batchVertices.data(), batchVertices.size() * sizeof(GeometryBatchVertex));
        vertexBuffer->GetRHIBuffer()->UnMap();

        primitive->args = {args};
    }

    void GeometryRenderer::UpdateTransform(const Matrix4 &matrix)
    {
        ubo->WriteT(0, matrix);
        ubo->WriteT(sizeof(Matrix4), matrix.InverseTranspose());
    }

    void GeometryRenderer::ResetPrimitive(const RDGfxTechPtr &tech)
    {
        primitive->batches[0].technique = tech;
    }

    void GeometryRenderer::ResizeVertex(uint32_t size)
    {
        batchVertices.resize(size);
        vertexCapacity = size;
        vertexBuffer = new Buffer();
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

        AddVertex({ToVec4(quad.v[0]), ToVec4(normal), currentColor});
        AddVertex({ToVec4(quad.v[1]), ToVec4(normal), currentColor});
        AddVertex({ToVec4(quad.v[2]), ToVec4(normal), currentColor});

        AddVertex({ToVec4(quad.v[2]), ToVec4(normal), currentColor});
        AddVertex({ToVec4(quad.v[3]), ToVec4(normal), currentColor});
        AddVertex({ToVec4(quad.v[0]), ToVec4(normal), currentColor});

        args.vertexCount += 6;
        return *this;
    }

    GeometryRenderer &GeometryRenderer::DrawAABB(const AABB &aabb)
    {
        //
        currentColor = Color(1.f, 0.f, 0.f, 1.f);
        AddVertex({Vector4{aabb.max.x, aabb.min.y, aabb.min.z, 1.0f}, ToVec4(-VEC3_Z), currentColor});
        AddVertex({Vector4{aabb.min.x, aabb.min.y, aabb.min.z, 1.0f}, ToVec4(-VEC3_Z), currentColor});
        AddVertex({Vector4{aabb.min.x, aabb.max.y, aabb.min.z, 1.0f}, ToVec4(-VEC3_Z), currentColor});
        AddVertex({Vector4{aabb.min.x, aabb.max.y, aabb.min.z, 1.0f}, ToVec4(-VEC3_Z), currentColor});
        AddVertex({Vector4{aabb.max.x, aabb.max.y, aabb.min.z, 1.0f}, ToVec4(-VEC3_Z), currentColor});
        AddVertex({Vector4{aabb.max.x, aabb.min.y, aabb.min.z, 1.0f}, ToVec4(-VEC3_Z), currentColor});

        AddVertex({Vector4{aabb.min.x, aabb.min.y, aabb.max.z, 1.0f}, ToVec4(VEC3_Z), currentColor});
        AddVertex({Vector4{aabb.max.x, aabb.min.y, aabb.max.z, 1.0f}, ToVec4(VEC3_Z), currentColor});
        AddVertex({Vector4{aabb.max.x, aabb.max.y, aabb.max.z, 1.0f}, ToVec4(VEC3_Z), currentColor});
        AddVertex({Vector4{aabb.max.x, aabb.max.y, aabb.max.z, 1.0f}, ToVec4(VEC3_Z), currentColor});
        AddVertex({Vector4{aabb.min.x, aabb.max.y, aabb.max.z, 1.0f}, ToVec4(VEC3_Z), currentColor});
        AddVertex({Vector4{aabb.min.x, aabb.min.y, aabb.max.z, 1.0f}, ToVec4(VEC3_Z), currentColor});

        //
        currentColor = Color(0.f, 1.f, 0.f, 1.f);
        AddVertex({Vector4{aabb.min.x, aabb.min.y, aabb.min.z, 1.0f}, ToVec4(-VEC3_X), currentColor});
        AddVertex({Vector4{aabb.min.x, aabb.min.y, aabb.max.z, 1.0f}, ToVec4(-VEC3_X), currentColor});
        AddVertex({Vector4{aabb.min.x, aabb.max.y, aabb.max.z, 1.0f}, ToVec4(-VEC3_X), currentColor});
        AddVertex({Vector4{aabb.min.x, aabb.max.y, aabb.max.z, 1.0f}, ToVec4(-VEC3_X), currentColor});
        AddVertex({Vector4{aabb.min.x, aabb.max.y, aabb.min.z, 1.0f}, ToVec4(-VEC3_X), currentColor});
        AddVertex({Vector4{aabb.min.x, aabb.min.y, aabb.min.z, 1.0f}, ToVec4(-VEC3_X), currentColor});

        AddVertex({Vector4{aabb.max.x, aabb.min.y, aabb.min.z, 1.0f}, ToVec4(VEC3_X), currentColor});
        AddVertex({Vector4{aabb.max.x, aabb.max.y, aabb.min.z, 1.0f}, ToVec4(VEC3_X), currentColor});
        AddVertex({Vector4{aabb.max.x, aabb.max.y, aabb.max.z, 1.0f}, ToVec4(VEC3_X), currentColor});
        AddVertex({Vector4{aabb.max.x, aabb.max.y, aabb.max.z, 1.0f}, ToVec4(VEC3_X), currentColor});
        AddVertex({Vector4{aabb.max.x, aabb.min.y, aabb.max.z, 1.0f}, ToVec4(VEC3_X), currentColor});
        AddVertex({Vector4{aabb.max.x, aabb.min.y, aabb.min.z, 1.0f}, ToVec4(VEC3_X), currentColor});

        //
        currentColor = Color(0.f, 0.f, 1.f, 1.f);
        AddVertex({Vector4{aabb.min.x, aabb.min.y, aabb.min.z, 1.0f}, ToVec4(-VEC3_Y), currentColor});
        AddVertex({Vector4{aabb.max.x, aabb.min.y, aabb.min.z, 1.0f}, ToVec4(-VEC3_Y), currentColor});
        AddVertex({Vector4{aabb.max.x, aabb.min.y, aabb.max.z, 1.0f}, ToVec4(-VEC3_Y), currentColor});
        AddVertex({Vector4{aabb.max.x, aabb.min.y, aabb.max.z, 1.0f}, ToVec4(-VEC3_Y), currentColor});
        AddVertex({Vector4{aabb.min.x, aabb.min.y, aabb.max.z, 1.0f}, ToVec4(-VEC3_Y), currentColor});
        AddVertex({Vector4{aabb.min.x, aabb.min.y, aabb.min.z, 1.0f}, ToVec4(-VEC3_Y), currentColor});

        AddVertex({Vector4{aabb.min.x, aabb.max.y, aabb.min.z, 1.0f}, ToVec4(VEC3_Y), currentColor});
        AddVertex({Vector4{aabb.min.x, aabb.max.y, aabb.max.z, 1.0f}, ToVec4(VEC3_Y), currentColor});
        AddVertex({Vector4{aabb.max.x, aabb.max.y, aabb.max.z, 1.0f}, ToVec4(VEC3_Y), currentColor});
        AddVertex({Vector4{aabb.max.x, aabb.max.y, aabb.max.z, 1.0f}, ToVec4(VEC3_Y), currentColor});
        AddVertex({Vector4{aabb.max.x, aabb.max.y, aabb.min.z, 1.0f}, ToVec4(VEC3_Y), currentColor});
        AddVertex({Vector4{aabb.min.x, aabb.max.y, aabb.min.z, 1.0f}, ToVec4(VEC3_Y), currentColor});

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
        }
        batchVertices[currentVertex] = vtx;
        batchVertices[currentVertex++].position += currentCenter;
    }

} // namespace sky
