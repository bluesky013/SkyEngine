//
// Created by Zach Lee on 2022/7/19.
//

#include <render/shapes/RenderShape.h>

namespace sky {

    static RDBufferViewPtr CopyData(RDBufferPtr& buffer, const std::vector<float>& values, uint64_t& offset, uint32_t stride)
    {
        uint64_t size = values.size() * sizeof(float);
        auto viewPtr = std::make_shared<BufferView>(buffer, stride, offset);
        buffer->Write(reinterpret_cast<const uint8_t*>(values.data()), size, offset);
        offset += size;
        return viewPtr;
    }

    RDMeshPtr RenderShape::CreateMesh(RDMaterialPtr material)
    {
        auto mesh = std::make_shared<Mesh>();
        Mesh::Builder builder(*mesh);

        SubMesh subMesh = {};
        subMesh.drawData = drawData;
        subMesh.material = material;
        subMesh.aabb = aabb;

        for (auto& vb : vertexBuffers) {
            builder.AddVertexBuffer(vb);
        }

        for (auto& desc : vertexDescriptions) {
            builder.AddVertexDesc(desc);
        }

        builder.AddSubMesh(subMesh);

        return mesh;
    }

    void Plane::Init()
    {
        Vector3 pos1 = {-0.5f, 0.f,  0.5f};
        Vector3 pos2 = {-0.5f, 0.f, -0.5f};
        Vector3 pos3 = { 0.5f, 0.f, -0.5f};
        Vector3 pos4 = { 0.5f, 0.f,  0.5f};

        Vector2 uv1 = {0.f, 1.f};
        Vector2 uv2 = {0.f, 0.f};
        Vector2 uv3 = {1.f, 0.f};
        Vector2 uv4 = {1.f, 1.f};

        Vector3 tangent1 = {0.f, 0.f, 0.f};
        Vector3 tangent2 = {0.f, 0.f, 0.f};

        Vector3 normal = {0.f, 1.f, 0.f};

        Vector3 edge1 = pos2 - pos1;
        Vector3 edge2 = pos3 - pos1;
        Vector2 deltaUV1 = uv2 - uv1;
        Vector2 deltaUV2 = uv3 - uv1;

        float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

        tangent1.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
        tangent1.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
        tangent1.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
        tangent1 = glm::normalize(tangent1);

        edge1 = pos3 - pos1;
        edge2 = pos4 - pos1;
        deltaUV1 = uv3 - uv1;
        deltaUV2 = uv4 - uv1;

        f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

        tangent2.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
        tangent2.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
        tangent2.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
        tangent2 = glm::normalize(tangent2);

        std::vector<float> positions = {
            pos1.x, pos1.y, pos1.z, 1.f,
            pos2.x, pos2.y, pos2.z, 1.f,
            pos3.x, pos3.y, pos3.z, 1.f,

            pos1.x, pos1.y, pos1.z, 1.f,
            pos3.x, pos3.y, pos3.z, 1.f,
            pos4.x, pos4.y, pos4.z, 1.f
        };

        std::vector<float> normals = {
            normal.x, normal.y, normal.z, 1.f,
            normal.x, normal.y, normal.z, 1.f,
            normal.x, normal.y, normal.z, 1.f,

            normal.x, normal.y, normal.z, 1.f,
            normal.x, normal.y, normal.z, 1.f,
            normal.x, normal.y, normal.z, 1.f,
        };

        std::vector<float> tangents {
            tangent1.x, tangent1.y, tangent1.z, 1.f,
            tangent1.x, tangent1.y, tangent1.z, 1.f,
            tangent1.x, tangent1.y, tangent1.z, 1.f,

            tangent2.x, tangent2.y, tangent2.z, 1.f,
            tangent2.x, tangent2.y, tangent2.z, 1.f,
            tangent2.x, tangent2.y, tangent2.z, 1.f,
        };

        std::vector<float> colors = {
            1.f, 1.f, 1.f, 1.f,
            1.f, 1.f, 1.f, 1.f,
            1.f, 1.f, 1.f, 1.f,

            1.f, 1.f, 1.f, 1.f,
            1.f, 1.f, 1.f, 1.f,
            1.f, 1.f, 1.f, 1.f,
        };

        std::vector<float> uvs = {
            uv1.x, uv1.y,
            uv2.x, uv2.y,
            uv3.x, uv3.y,

            uv1.x, uv1.y,
            uv3.x, uv3.y,
            uv4.x, uv4.y,
        };

        Buffer::Descriptor descriptor = {};
        descriptor.size = (positions.size() + normals.size() + tangents.size() + colors.size() + uvs.size()) * sizeof(float);
        descriptor.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        descriptor.memory = VMA_MEMORY_USAGE_GPU_ONLY;
        descriptor.keepCPU = true;

        uint64_t offset = 0;
        buffer = std::make_shared<Buffer>(descriptor);
        buffer->InitRHI();

        vertexBuffers.emplace_back(CopyData(buffer, positions, offset, 16));
        vertexBuffers.emplace_back(CopyData(buffer, normals, offset, 16));
        vertexBuffers.emplace_back(CopyData(buffer, tangents, offset, 16));
        vertexBuffers.emplace_back(CopyData(buffer, colors, offset, 16));
        vertexBuffers.emplace_back(CopyData(buffer, uvs, offset, 8));
        buffer->Update(true);

        vertexDescriptions.emplace_back(VertexDesc{0, 0, VK_FORMAT_R32G32B32A32_SFLOAT});
        vertexDescriptions.emplace_back(VertexDesc{1, 0, VK_FORMAT_R32G32B32A32_SFLOAT});
        vertexDescriptions.emplace_back(VertexDesc{2, 0, VK_FORMAT_R32G32B32A32_SFLOAT});
        vertexDescriptions.emplace_back(VertexDesc{3, 0, VK_FORMAT_R32G32B32A32_SFLOAT});
        vertexDescriptions.emplace_back(VertexDesc{4, 0, VK_FORMAT_R32G32_SFLOAT});

        drawData.firstVertex = 0;
        drawData.vertexCount = 6;

        aabb.min = Vector3 {-0.5f, 0.f, -0.5f};
        aabb.max = Vector3 {0.5f, 0.f, 0.5f};
    }

}

